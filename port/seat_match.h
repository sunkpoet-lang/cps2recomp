// SPDX-License-Identifier: GPL-3.0-only
// A backend-neutral seat match: the host-star room, an N-player rollback
// session and membership transitions (a PC joining or leaving during the match, a lone host waiting), for any board
// that can save and load its own state, run one frame with 2-4 input lanes and give a semantic hash of its state.
// The CPS port's Online has the same cycle for its own board; Sunset Riders is the first backend that uses this one.
//
// Rules kept from the CPS path:
// - a PC never loads another PC's state: a joining PC rebuilds the transition's frame from its own start and the
//   confirmed inputs, and every other PC loads its own checkpoint of that frame;
// - nothing of a proposed roster routes game data before the transition's commit;
// - frames are global (the frame the first session began at is 0); each session's frame 0 is a transition's frame.
#pragma once
#include "rollback_star.h"
#include "rollback_session.h"
#include "rollback_session_owner.h"
#include "player_inputs.h"
#include "membership_checkpoints.h"
#include "membership_bootstrap.h"
#include <zlib.h>
#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace recomp::match {

// What the board gives the match. Every state is this PC's own.
struct Backend {
    std::function<bool(std::vector<uint8_t> &state)> save;          // the board's whole state now
    std::function<bool(std::span<const uint8_t> state)> load;       // put back a state this PC saved
    std::function<bool(std::span<const uint16_t> lanes, bool again)> frame; // one frame; again: rolled back (unseen, unheard)
    std::function<uint32_t()> hash;                                  // the game's semantic fingerprint now
    // The fingerprint is the CRC of the state save gives (a board whose whole state is its game: no second packing)
    bool hashSaved = false;
    // (optional) What the port shows and keeps beside the game: its replay of each session, the match's presentation.
    // Frames here are the session's own (0 at its start); none is called while a joining PC rebuilds its state.
    std::function<void()> begun;                                              // a session began (Session(), RecordingBase())
    std::function<bool(int frame)> saved;                                     // after a save of the session
    std::function<bool(int frame)> loaded;                                    // after a load of the session
    std::function<void(int frame, std::span<const uint16_t> lanes)> before;  // before a frame (not one run ahead)
    std::function<void(int frame, std::span<const uint16_t> lanes, bool again)> after; // after it
    std::function<void(uint64_t frames)> ending;                              // the session goes: keep its first frames
    std::function<void()> applied;                                            // a transition applied (the board is its frame)
};

struct Options {
    bool host = false;
    uint16_t port = 7845;
    std::string address;            // (a guest: the host's)
    unsigned capacity = 2;          // the game's players (2-4)
    unsigned locals = 1;            // controllers at this PC (1-2)
    unsigned startPlayers = 0;      // seats filled before the first session (0: all)
    bool lateJoins = false;         // (host) players join and leave during the match
    bool epochs = true;             // membership epochs (without: a fixed room, a PC leaving ends the match)
    std::string name = "Player";
    std::vector<uint8_t> profile;   // what must match between PCs: game, ROM set, build, rules
    std::string build;              // the program's SHA-256 (64 hex), for the bootstrap identity
    unsigned delay = 1, prediction = 8, timeoutMs = 5000;
    uint32_t stateBytes = 0;        // the largest state the board saves, with room to spare
    net::BindScope scope = net::BindScope::Any;
    std::FILE *checkpointLog = nullptr; // (global frame and hash of each saved frame, "# epoch" lines)
};

class SeatMatch {
public:
    enum class Status { Waiting, Connecting, Playing, Ended };

    ~SeatMatch() { Stop(); }

    bool Start(const Options &options, Backend backend, std::string &error) {
        if (star_) { error = "The seat match has already started"; return false; }
        if (options.capacity < 2 || options.capacity > 4 || options.locals < 1 || options.locals > 2 ||
            (options.host && options.locals >= options.capacity) || !options.stateBytes) {
            error = "Invalid seat match configuration"; return false;
        }
        if (options.lateJoins && (!options.host || !options.epochs)) { error = "Late joins need the host of an epoch-framed seat room"; return false; }
        if (options.startPlayers && !options.epochs) { error = "The initial player threshold requires epochs"; return false; }
        if (!backend.save || !backend.load || !backend.frame || !backend.hash) { error = "The board's seat backend is incomplete"; return false; }
        o_ = options; b_ = std::move(backend);
        if (!b_.save(bootstrapState_) || bootstrapState_.empty()) { error = "The board's starting state could not be kept"; return false; }
        profile_ = o_.profile;
        Append("|seats-2");
        if (o_.epochs) Append("|epochs-1|");
        if (o_.startPlayers) Append("|initial-" + std::to_string(o_.startPlayers));
        identity_ = net::MembershipBootstrap(o_.build, profile_, bootstrapState_);
        if (!identity_ && o_.epochs) { error = "Epoch preparation needs a verified local bootstrap identity (the program's SHA-256)"; return false; }
        if (identity_) {
            Append("|bootstrap-1|");
            for (auto byte : *identity_) { profile_.push_back("0123456789abcdef"[byte >> 4]); profile_.push_back("0123456789abcdef"[byte & 15]); }
        }
        star_ = std::make_unique<net::StarRoom>();
        std::random_device random;
        uint64_t token = (uint64_t(random()) << 32) ^ uint64_t(random()); if (!token) token = 1;
        if (!star_->Open(o_.host, o_.port, o_.address, token, o_.capacity, o_.locals, o_.name, profile_, error, o_.scope, o_.startPlayers)) {
            star_.reset(); return false;
        }
        net::StarAdapter::active = star_.get();
        started_ = Now(); status_ = Status::Waiting;
        message_ = "Waiting for " + std::to_string(o_.capacity) + " players";
        return true;
    }

    void Stop() {
        if (!star_) return;
        if (session_ && b_.ending) b_.ending(UINT64_MAX);
        session_.Reset();
        if (net::StarAdapter::active == star_.get()) net::StarAdapter::active = nullptr;
        star_.reset();
    }

    // One tick: the room, the transitions, and the session's frames with this PC's controls (one or two lanes).
    // Returns how many frames were seen this tick.
    int Tick(std::span<const uint16_t> local) {
        if (!star_ || status_ == Status::Ended) return 0;
        const uint64_t now = Now();
        shown_ = 0;
        star_->Pump(now); star_->Tick(now);
        if (!star_->Refusal().empty()) { End(star_->Refusal(), true); return 0; }
        if (!star_->Ready()) {
            if (!star_->IsHost() && star_->ProvisionalId() && star_->Mask() && !star_->Membership()) {
                if (!star_->AttachCandidate()) { End("The late seat offer could not be attached", true); return 0; }
                Log("late join: seats %u offered at epoch %llu", unsigned(star_->Mask()), ull(star_->Membership()->Epoch().Epoch()));
            }
            if (!star_->Candidate()) { if (now - started_ >= 60000) End("Seat admission timed out after 60 seconds", true); return 0; }
        }
        if (o_.epochs) {
            if (!star_->Membership()) {
                if (!star_->AttachMembership() || !star_->EnableEpochFraming()) { End("The epoch-framed room could not attach", true); return 0; }
                Log("epoch-framed seat transport attached (epoch 1)");
                if (o_.lateJoins) { if (!star_->EnableLateReservations()) { End("Late joins could not be enabled", true); return 0; } Log("late joins enabled"); }
            }
            while (auto e = star_->TakeMembership()) { Handle(*e); if (status_ == Status::Ended) return 0; }
            if (auto e = star_->Membership()->Tick(now); e.accepted || e.send) Handle(e);
            if (status_ == Status::Ended) return 0;
            Service(now);
            if (status_ == Status::Ended || !star_->Membership()->MayAdvance() || !star_->Ready()) return 0;
        }
        if (o_.epochs && !session_ && star_->IsHost() && star_->Filled() == star_->Mask()) {
            if (message_ != "Waiting for players to join") { message_ = "Waiting for players to join"; Log("alone in the room; waiting for players to join"); }
            return 0;
        }
        if (!session_ && !CreateSession()) { End("The N-player rollback session could not start", true); return 0; }
        rollback::Pads pads{};
        if (!rollback::LocalInputs(local, star_->Mask(), o_.capacity, pads)) { End("The local controls do not match the allocated seats", true); return 0; }
        gekko_network_poll(session_.Get());
        for (unsigned seat = 0; seat < o_.capacity; ++seat) if (star_->SdkLocal(seat)) gekko_add_local_input(session_.Get(), actors_[seat], &pads[seat]);
        Run();
        return shown_;
    }

    Status GetStatus() const { return status_; }
    bool Failed() const { return failed_; }
    const std::string &Message() const { return message_; }
    unsigned Players() const { return o_.capacity; }
    unsigned Primary() const { return star_ ? star_->Primary() : 0; }
    uint8_t Filled() const { return star_ ? star_->Filled() : 0; }
    std::array<std::string, 4> Names() const { return star_ ? star_->Names() : std::array<std::string, 4>{}; }
    uint64_t Frame() const { return base_ + sessionFrame_; }   // global frames run (forward)
    uint64_t EpochBase() const { return base_; }
    uint64_t Epoch() const { return star_ && star_->Membership() ? star_->Membership()->Epoch().Epoch() : 0; }
    std::span<const uint8_t> Profile() const { return profile_; }
    net::StarRoom *Room() const { return star_.get(); }
    GekkoSession *Session() const { return session_ ? session_.Get() : nullptr; }
    unsigned RecordingBase() const { return recordingBase_; }
    // Every confirmed input of the match from its global frame 0, lanes interleaved (a replay of the whole match:
    // the sessions before this one, then this one's confirmed frames). Before Stop.
    bool ConfirmedHistory(std::vector<uint16_t> &out) {
        if (!session_) { out = history_; return history_.size() == size_t(base_) * o_.capacity; }
        if (!recordingBase_) return false;
        unsigned length = 0; const auto *data = gekko_peek_recording(session_.Get(), &length); if (!data) return false;
        const auto confirmed = net::ConfirmedRecording(std::span<const uint8_t>(data, length), recordingBase_, o_.capacity);
        return ConfirmedPrefix(base_ + confirmed.size() / (o_.capacity * sizeof(uint16_t)), out);
    }
    // (how far this PC runs ahead of the others: a window slows a little when over one frame)
    float FramesAhead() const { return session_ && status_ == Status::Playing ? gekko_frames_ahead(session_.Get()) : 0.0f; }

private:
    using ull = unsigned long long;
    struct Transition { bool active = false, staged = false, verified = false, journal = false; uint64_t id = 0, frame = 0, began = 0; };

    Options o_; Backend b_;
    std::unique_ptr<net::StarRoom> star_;
    rollback::Session session_;
    std::array<int, 4> actors_{-1, -1, -1, -1};
    std::vector<uint8_t> scratch_, bootstrapState_, profile_;
    std::optional<net::ConfirmedInputStaging::Identity> identity_;
    net::MembershipCheckpoints checkpoints_;
    unsigned recordingBase_ = 0;
    uint64_t base_ = 0, sessionFrame_ = 0, started_ = 0;
    std::vector<uint16_t> history_, joinInputs_;
    std::optional<unsigned> leaveRequest_;
    Transition tr_;
    Status status_ = Status::Waiting;
    bool failed_ = false;
    std::string message_;
    int shown_ = 0;

    static uint64_t Now() { return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
    void Append(const std::string &s) { profile_.insert(profile_.end(), s.begin(), s.end()); }
    template <typename... A> static void Log(const char *format, A... a) {
        std::fprintf(stderr, "online: "); std::fprintf(stderr, format, a...); std::fprintf(stderr, "\n");
    }
    void End(const std::string &why, bool failed) {
        if (status_ == Status::Ended) return;
        status_ = Status::Ended; failed_ = failed; message_ = why; Log("%s", why.c_str());
    }

    uint32_t Fingerprint() {
        if (!b_.hashSaved) return b_.hash();
        std::vector<uint8_t> state;
        if (!b_.save(state)) return 0;
        return uint32_t(crc32(0, state.data(), uInt(state.size())));
    }
    uint32_t lastSaved_ = 0;

    bool CreateSession() {
        GekkoConfig config{};
        config.num_players = static_cast<unsigned char>(o_.capacity);
        config.max_input_prediction_window = static_cast<unsigned char>(o_.prediction);
        config.input_size = sizeof(uint16_t); config.state_size = o_.stateBytes;
        config.desync_detection = true; config.check_distance = 10;
        if (!session_.Start(GekkoGameSession, config, &net::StarAdapter::callbacks, o_.timeoutMs)) return false;
        const unsigned own = star_->Primary(); int remote = -1;
        for (unsigned seat = 0; seat < o_.capacity; ++seat) {
            const bool local = star_->SdkLocal(seat); const unsigned owner = star_->RoutingOwner(seat);
            if (owner >= o_.capacity) return false;
            auto at = net::StarAdapter::Address(owner);
            actors_[seat] = session_.AddActor(local ? GekkoLocalPlayer : GekkoRemotePlayer, local ? nullptr : &at);
            if (actors_[seat] < 0) return false;
            if (local) { if (!session_.Delay(actors_[seat], o_.delay)) return false; }
            else if (remote < 0) remote = actors_[seat];
        }
        if (remote < 0 || own >= o_.capacity) return false;
        if (!gekko_start_recording(session_.Get(), false, true)) return false;
        gekko_peek_recording(session_.Get(), &recordingBase_);
        sessionFrame_ = 0; status_ = Status::Connecting;
        Log("seats: local %u, players %u, mask %u, local controls %u, epoch %llu from frame %llu", own, o_.capacity, unsigned(star_->Mask()), o_.locals, ull(Epoch()), ull(base_));
        if (b_.begun) b_.begun();
        return true;
    }

    void Run() {
        Events();
        if (status_ == Status::Ended) return;
        rollback::Callbacks callbacks;
        callbacks.active = [&] { return status_ != Status::Ended; };
        callbacks.save = [&](int frame, std::vector<uint8_t> &state) {
            if (!b_.save(state)) { End("The board's state could not be saved", true); return false; }
            lastSaved_ = b_.hashSaved ? uint32_t(crc32(0, state.data(), uInt(state.size()))) : b_.hash();
            if (!checkpoints_.Save(frame, lastSaved_, state)) { End("The local checkpoint exceeds its bounded cache", true); return false; }
            if (b_.saved && !b_.saved(frame)) { End("The match's presentation could not be kept", true); return false; }
            return true;
        };
        callbacks.checksum = [&](int frame) {
            const uint32_t hash = lastSaved_; // (the save just before it, for this same frame)
            if (o_.checkpointLog) std::fprintf(o_.checkpointLog, "%lld %08x\n", static_cast<long long>(frame) + static_cast<long long>(base_), hash);
            return hash;
        };
        callbacks.load = [&](int frame, std::span<const uint8_t> bytes) {
            if (b_.loaded && !b_.loaded(frame)) { End("The match presentation checkpoint is unavailable", true); return false; }
            if (!b_.load(bytes)) { End("A state the session kept did not load", true); return false; }
            return callbacks.active();
        };
        callbacks.advance = [&](rollback::Frame frame, std::span<const uint8_t> inputs) {
            rollback::Pads pads{};
            if (!rollback::DecodeInputs(inputs, o_.capacity, pads)) return false;
            const bool again = frame.rollback || frame.runahead;
            const std::span<const uint16_t> lanes(pads.data(), o_.capacity);
            if (!frame.runahead && b_.before) b_.before(frame.number, lanes);
            if (!b_.frame(lanes, again)) { End("The board refused the player input lanes", true); return false; }
            if (!frame.runahead && b_.after) b_.after(frame.number, lanes, again);
            if (!again) { ++shown_; sessionFrame_ = std::max<uint64_t>(sessionFrame_, uint64_t(frame.number) + 1); }
            return callbacks.active();
        };
        callbacks.error = [&](rollback::Error e) {
            End(e == rollback::Error::StateCapacity ? "The board's state outgrew what the session keeps" : "The rollback session supplied invalid buffers", true);
        };
        rollback::Step(session_.Get(), {o_.capacity, sizeof(uint16_t), o_.stateBytes}, scratch_, callbacks);
        Events();
    }

    void Events() {
        if (!session_) return;
        int count = 0;
        GekkoSessionEvent **events = gekko_session_events(session_.Get(), &count);
        for (int i = 0; i < count && status_ != Status::Ended; ++i) {
            const auto &event = *events[i];
            switch (event.type) {
            case GekkoPlayerSyncing:
                message_ = "Connecting " + std::to_string(event.data.syncing.current) + "/" + std::to_string(event.data.syncing.max);
                break;
            case GekkoSessionStarted:
                if (status_ != Status::Playing) Log("playing");
                status_ = Status::Playing; message_.clear(); break;
            case GekkoPlayerDisconnected: {
                const int handle = event.data.disconnected.handle;
                if (!o_.epochs) { End("A player disconnected; membership epochs are not enabled", true); break; }
                for (unsigned seat = 0; seat < o_.capacity; ++seat) if (actors_[seat] == handle) {
                    const unsigned owner = star_->OwnerOfSeat(seat);
                    if (!star_->IsHost() && owner == 0) { End("The host left the match", false); break; }
                    if (star_->IsHost() && owner && owner < 4 && !leaveRequest_) {
                        if (!o_.lateJoins) { End("A player left the match", false); break; }
                        leaveRequest_ = owner;
                    }
                }
                if (status_ != Status::Ended) Log("a player's PC stopped answering (handle %d)", handle);
                break;
            }
            case GekkoDesyncDetected: {
                const auto &d = event.data.desynced;
                Log("desync at frame %d (here %08x, the other side %08x)", d.frame, d.local_checksum, d.remote_checksum);
                End("Desync at frame " + std::to_string(d.frame), true);
                break;
            }
            default: break;
            }
        }
    }

    // ---- membership transitions (as the CPS path's ServiceTransition / ApplyTransition)

    void Broadcast(const net::MembershipEpoch::Effect &e) {
        if (!e.send) return;
        auto *ch = star_->Membership();
        if (star_->IsHost()) {
            const auto &p = e.send->proposal; const uint8_t to = uint8_t(p.oldMembers | p.newMembers);
            for (unsigned peer = 1; peer < 4; ++peer) if ((to & (1u << peer)) && ch->Pinned(peer)) star_->SendMembership(peer, e);
            if (e.send->kind == net::MembershipEpoch::Kind::Prepare) star_->SendPending();
        } else star_->SendMembership(0, e);
    }

    void Handle(const net::MembershipEpoch::Effect &e) {
        if (e.failed) { End("The change of players failed; the match can't go on", true); return; }
        if (e.departed) { End("This PC left the match", false); return; }
        if (e.resumeOld) {
            Log("membership transition %llu abandoned; the match goes on as it was", ull(tr_.id));
            star_->CancelJournals(); star_->CancelPending(); tr_ = {}; joinInputs_.clear();
            if (star_->Candidate()) { End("The host abandoned this PC's join", true); return; }
        }
        if (e.apply) {
            const bool ok = Apply();
            auto published = star_->Membership()->Publish(ok);
            if (!ok || published.failed) { End("This PC couldn't take the new players' state", true); return; }
            Broadcast(published);
            return;
        }
        Broadcast(e);
    }

    bool ConfirmedPrefix(uint64_t frames, std::vector<uint16_t> &out) {
        if (frames < base_ || history_.size() != size_t(base_) * o_.capacity) return false;
        out = history_;
        if (frames == base_) return true;
        if (!session_ || !recordingBase_) return false;
        unsigned length = 0; const auto *data = gekko_peek_recording(session_.Get(), &length);
        std::vector<uint16_t> own;
        if (!data || !net::ExportConfirmedPrefix(std::span<const uint8_t>(data, length), recordingBase_, o_.capacity, frames - base_, own)) return false;
        out.insert(out.end(), own.begin(), own.end());
        return true;
    }

    bool ChooseCheckpoint(uint64_t &frame, uint32_t &fingerprint) {
        if (!session_) { frame = base_; fingerprint = Fingerprint(); return true; }
        if (!recordingBase_) return false;
        unsigned length = 0; const auto *data = gekko_peek_recording(session_.Get(), &length); if (!data) return false;
        const auto confirmed = net::ConfirmedRecording(std::span<const uint8_t>(data, length), recordingBase_, o_.capacity);
        const uint64_t count = confirmed.size() / (o_.capacity * sizeof(uint16_t));
        for (uint64_t k = count; k > 0 && k + net::MembershipCheckpoints::maxEntries > count; --k)
            if (const auto *own = checkpoints_.Find(k)) { frame = base_ + k; fingerprint = own->fingerprint; return true; }
        return false;
    }

    bool Rebuild(std::span<const uint16_t> inputs, uint64_t frames, uint32_t &hash) {
        if (bootstrapState_.empty() || inputs.size() != size_t(frames) * o_.capacity) return false;
        if (!b_.load(bootstrapState_)) return false;
        for (uint64_t f = 0; f < frames; ++f) if (!b_.frame(inputs.subspan(size_t(f) * o_.capacity, o_.capacity), true)) return false;
        hash = Fingerprint();
        return true;
    }

    void Service(uint64_t now) {
        auto *ch = star_->Membership(); if (!ch || ch->Failed()) return;
        const bool host = star_->IsHost();
        if (host && !tr_.active && ch->MayAdvance() && (session_ || star_->Filled() == star_->Mask()) && (leaveRequest_ || star_->ReservationId())) {
            uint64_t frame = 0; uint32_t fingerprint = 0;
            if (!ChooseCheckpoint(frame, fingerprint)) return;
            const uint64_t id = ch->Epoch().Current().id + 1;
            const bool leaving = leaveRequest_.has_value();
            auto pending = leaving ? star_->BeginLeave(*leaveRequest_, id) : star_->BeginJoin(id);
            leaveRequest_.reset();
            if (!pending) return;
            auto roster = star_->PendingMembership(id);
            if (!roster) { star_->CancelPending(); return; }
            auto begun = ch->Begin(roster->members, frame, fingerprint, now);
            if (!begun.accepted || !ch->Stage(*roster)) { star_->CancelPending(); if (begun.accepted) Handle(ch->Abort()); return; }
            tr_ = {true, true, false, false, id, frame, now};
            Log("membership transition %llu: %s PC %d at frame %llu", ull(id), leaving ? "leaving" : "joining", leaving ? pending->leaving : pending->joining, ull(frame));
            Handle(begun); if (status_ == Status::Ended) return;
            if (!leaving) {
                std::vector<uint16_t> prefix;
                if (!ConfirmedPrefix(frame, prefix) || !star_->StartJournal(unsigned(pending->joining), *identity_, o_.capacity, now, prefix)) {
                    Log("the joining PC's inputs couldn't be prepared"); Handle(ch->Abort()); return;
                }
            }
            Handle(ch->Verify(frame, fingerprint, now));
            return;
        }
        if (host || ch->Epoch().State() != net::MembershipEpoch::Stage::Preparing) return;
        const auto &p = ch->Epoch().Current();
        if (!tr_.active || tr_.id != p.id) {
            tr_ = {true, false, false, false, p.id, p.frame, now}; star_->AdoptPendingId(p.id);
            Log("membership transition %llu proposed at frame %llu", ull(p.id), ull(p.frame));
        }
        if (!tr_.staged) { auto roster = star_->PendingMembership(p.id); if (!roster || !ch->Stage(*roster)) return; tr_.staged = true; }
        if (tr_.verified) return;
        if (star_->Candidate()) {
            if (!tr_.journal) {
                if (!star_->StartJournal(0, *identity_, o_.capacity, now)) { End("The join couldn't ask for the match's inputs", true); return; }
                tr_.journal = true;
            }
            const auto *journal = star_->Journal(0);
            if (!journal || journal->Failed()) { End("The match's inputs didn't arrive", true); return; }
            if (!journal->Complete()) return;
            const auto inputs = journal->Inputs(); joinInputs_.assign(inputs.begin(), inputs.end());
            const auto began = Now(); uint32_t hash = 0;
            if (!Rebuild(joinInputs_, p.frame, hash)) { End("The match couldn't be rebuilt on this PC", true); return; }
            Log("late join: rebuilt %llu frames in %llu ms, state %08x (the host's %08llx)", ull(p.frame), ull(Now() - began), unsigned(hash), ull(p.fingerprint));
            tr_.verified = true;
            Handle(ch->Verify(p.frame, hash, Now()));
            return;
        }
        if (p.frame < base_) return;
        const auto *own = checkpoints_.Find(p.frame - base_);
        tr_.verified = true;
        if (!own) { Log("no own checkpoint at frame %llu for the transition", ull(p.frame)); return; }
        Handle(ch->Verify(p.frame, own->fingerprint, now));
    }

    bool Apply() {
        auto *ch = star_->Membership(); const auto &p = ch->Epoch().Current(); const uint64_t frame = p.frame;
        if (star_->Candidate()) {
            history_ = joinInputs_;
            if (history_.size() != size_t(frame) * o_.capacity) return false;
        } else if (!session_) {
            if (frame != base_ || history_.size() != size_t(frame) * o_.capacity) return false;
        } else {
            if (frame < base_) return false;
            const auto *own = checkpoints_.Find(frame - base_);
            std::vector<uint16_t> prefix;
            if (!own || !ConfirmedPrefix(frame, prefix)) return false;
            if (b_.ending) b_.ending(frame - base_); // (the session that ends: its first frames, up to the transition's)
            if (!b_.load(own->bytes)) return false;
            history_ = std::move(prefix);
        }
        if (b_.applied) b_.applied();
        session_.Reset(); actors_ = {-1, -1, -1, -1};
        if (!star_->PublishPending()) return false;
        base_ = frame; sessionFrame_ = 0; checkpoints_.Clear(); joinInputs_.clear();
        if (o_.checkpointLog) std::fprintf(o_.checkpointLog, "# epoch %llu from frame %llu\n", ull(p.nextEpoch), ull(frame));
        Log("membership transition %llu applied: epoch %llu from frame %llu, seats %u, after %llu ms", ull(p.id), ull(p.nextEpoch), ull(frame), unsigned(star_->Filled()), ull(Now() - tr_.began));
        tr_ = {};
        return true;
    }
};

} // namespace recomp::match

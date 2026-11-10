// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy and the fork's contributors. Part of cps2recomp (this fork), from neogeo-recomp's
// port/neogeo_online.h.
//
// Online play for a CPS game (this fork's; upstream's own is port/netplay.h of its main): two players, each with the whole board, kept the same by rollback (GekkoNet,
// BSD-2-Clause). Each side runs its own player's controls at once and predicts the other's; when the other's arrive
// and differ, the board goes back to the last frame both agree on (the save state: the machine and the game's sources' state) and runs the frames again,
// unseen and unheard. Each side compares a checksum of the board with the other's, and a difference ends the session.
//
//   <game> --host [PORT]         player 1, waiting on that UDP port (7845)
//   <game> --join HOST:PORT      player 2
//   <game> --delay N             the local controls held back N frames (0-8; 1): fewer frames to take back
//   <game> --watch HOST:PORT     (this fork) a spectator: the match as the host's game confirms it, a little behind
//                                (openspec change match-spectators)
//   <game> --stress              no network: GekkoNet's stress session, rolling back every frame to check that the
//                                game comes out the same (a game passes it before it is played online)
//
// The packets go over one UDP socket of the port's own (not GekkoNet's asio): the host takes the first player who
// says hello as its peer, from whatever address, which is what a tunnel (cps-tunel) needs, its packets coming from a
// local port of its own. Before GekkoNet starts, the two say hello with the game, a checksum of its ROM set and the
// program's version, and the features that change the game (the Game and Board pages'), and play only if they are
// the same. Online, the cheats, the custom soundtrack, run-ahead and the like are off (runtime/game.h, OnlineOff).
// (openspec change cps-online)
#pragma once

#include "match_flow.h"
#include "recovery.h"
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "chat_link.h"
#include "match_link.h"

namespace recomp::chat {
class Channel;
}

struct GekkoSession;

namespace port {

struct Port;

struct OnlineOptions {
    enum class Mode { Off, Host, Join, Stress, Watch } mode = Mode::Off;
    uint16_t port = 7845; // the host's, or the one --join goes to
    std::string host;     // --join's address
    int delay = 1;
    std::string name;     // (this fork) --nick: this player's name, said in the hello (ForkAde's in-game chat)
    bool matchFlow = false, arcadeWait = false; // explicit ForkAde presentation/solo wait; no wire change
    std::string checkpointLog; // save-event frame and board fingerprint only
    std::string flowLog; // optional save-event presentation counters; no game state bytes
    int chatLoss = 0;     // (this fork) --chat-loss PCT: (tests) that share of the chat's packets dropped as sent
    int spectators = 6;   // (this fork) the host's: how many may watch (0: none, the match refuses them)
    int spectatorDelay = 15; // (this fork) a spectator's: the frames it stays behind the confirmed match
    bool spectate = false;   // (this fork) --gekko-spectate: a --gekko-join that watches (the launcher's Watch)
};

// (this fork) The session also carries the in-game chat (ChatLink): ForkAde's docs/chat-protocol.md, its C and A
// packets on the session's socket, never GekkoNet's, and each player's name after a 0 byte in the hello, outside the
// part both sides compare
class Online : public ChatLink, public MatchLink {
public:
    enum class Status { Waiting, Connecting, Playing, Recovering, Ended };
    MatchView ReadMatch() const override;

    Online() = default;
    Online(const Online &) = delete;
    Online &operator=(const Online &) = delete;
    ~Online();

    // Opens the socket and the session; false with a message. The port's board must be as it powered on.
    bool Start(Port &port, const OnlineOptions &options, std::string &error);
    // One tick (a frame's time): the local player's controls (the stress session: both players'); the session's
    // frames are run on the port's board. Returns how many frames were run that are to be seen (0 or more).
    int Tick(const uint16_t local[2], bool advanceWaiting = true);
    bool ArcadeWaiting() const { return m_options.arcadeWait && m_options.mode == OnlineOptions::Mode::Host && !m_heard && m_status != Status::Ended; }
    Status GetStatus() const {
        return m_status;
    }
    bool Recovering() const { return m_recovery.Deadline()!=0 && m_status!=Status::Ended; }
    uint64_t Epoch() const { return m_recovery.Epoch(); }
    bool Failed() const {
        return m_failed;
    }
    // What the window says (waiting, connecting, a desync, the other player gone; "" while playing)
    const std::string &Message() const {
        return m_message;
    }
    // A line for a corner of the screen: the ping, the delay and the frames taken back
    std::string Stats() const;
    // How far this side is ahead of the other: the window waits a little longer when it is
    float FramesAhead() const;
    void Stop();

    // (this fork) ChatLink
    Said ChatSay(const std::string &text, std::string &line) override;
    std::vector<std::string> ChatHeard() override;
    std::string ChatName(int player) const override;
    int ChatLocal() const override;
    bool ChatHost() const override;
    bool ChatReady() const override;
    bool ChatOver() const override;
    std::vector<std::string> ChatUndelivered() const override;
    std::vector<Heard> ChatHeardLines() override;
    bool ChatSpectating() const override { return Spectating(); }

    // (the adapter's, for GekkoNet's callbacks)
    struct Socket;

    // (this fork) How many spectators are watching (the host's), and whether this side is one
    int Watching() const;
    bool Spectating() const { return m_options.mode == OnlineOptions::Mode::Watch; }

private:
    std::string Mismatch(const std::vector<uint8_t> &said, bool recoveryCompatible) const;
    void PumpChat();
    void RelayLine(int role, const std::string &name, const std::string &text, const std::string &except);
    std::vector<uint8_t> HelloOut() const;
    void SendStatus(int phase);
    void ApplyStatus();
    void WatchHellos();
    void AddSpectators();
    void SpectatorLeft(int handle);
    bool CreateSpectateSession();
    void UpdateFlowMessage();
    bool CreateGameSession();
    void SendControl(char kind,uint64_t epoch);
    void PrepareRecovery();
    void RequestRecovery();
    bool ResetRecoveryEpoch();
    void ApplyRecovery(const Recovery::Action& action);
    Recovery m_recovery;
    uint64_t m_localNonce=0,m_peerNonce=0,m_roomNonce=0;
    bool m_prepared=false,m_everPlayed=false;
    void PublishFlow();
    MatchFlow m_flow;
    void Pump();
    void SendHello();
    void Events();
    void Run();
    void End(const std::string &message, bool failed);

    Port *m_port = nullptr;
    OnlineOptions m_options;
    GekkoSession *m_session = nullptr;
    Socket *m_socket = nullptr;
    Status m_status = Status::Waiting;
    bool m_failed = false;
    std::string m_message;
    int m_local = 0, m_remote = 1; // the players' handles in the session
    std::vector<uint8_t> m_hello;  // what this side says hello with
    bool m_heard = false;          // the other side's hello has come, and it is the same game
    uint64_t m_ticks = 0;
    uint32_t m_stateSize = 0;
    std::vector<uint8_t> m_state;
    int m_shown = 0;
    int m_coinFrames = 0;
    std::unique_ptr<recomp::chat::Channel> m_chat; // (this fork: the in-game chat)
    std::string m_theirName;
    std::vector<uint8_t> m_arcadeState;
    uint64_t m_challengerUntil = 0;
    std::string m_lastFlowMessage;
    std::FILE* m_checkpointLog = nullptr;
    std::FILE* m_flowLog = nullptr;
    // (this fork: spectators) the host's: GekkoNet's name for each one (its address is the socket's), its nickname and handle (-1
    // until the match runs)
    struct Spectator {
        std::string name, nick;
        int handle = -1;
        std::shared_ptr<recomp::chat::Channel> own; // (its lines to the host, C and A)
        std::shared_ptr<class ChatRelay> relay;     // (the others' lines to it, V and U)
    };
    std::vector<Spectator> m_spectators;
    // (this fork: spectators' chat) the host's relay to the other player; the other player's and a spectator's from
    // the host
    std::shared_ptr<class ChatRelay> m_relay;
    // (this fork: spectators) what the host's last status said: its phase (0 waiting for a challenger, 1 connecting,
    // 2 playing, 3 reconnecting, 4 ended), how many watch, and the players' names
    int m_hostPhase = -1, m_seenWatching = 0;
    std::string m_statusNames[2];
};

} // namespace port

// SPDX-License-Identifier: GPL-3.0-only
// Backend-neutral initial seat admission and host-star Gekko transport.
#pragma once
#include "udp_transport.h"
#include "membership_channel.h"
#include <gekkonet.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <span>
#include <string>
#include <vector>
namespace recomp::net {
class StarRoom {
    struct Peer { bool used=false; Endpoint at{}; uint64_t token=0; uint8_t mask=0, ack=0; };
    Udp udp_; bool host_=false, ready_=false; unsigned capacity_=2, requested_=1, initial_=2;
    uint64_t token_=0,room_=0,next_=0; uint8_t primary_=255,mask_=0,filled_=0;
    Endpoint hostAt_{}; std::string name_,refusal_;std::vector<uint8_t>profile_;
    std::array<Peer,4>peers_{};std::array<uint8_t,4>owners_{255,255,255,255};std::array<std::string,4>names_{};
    std::map<uint64_t,uint64_t>refusedAt_;
    struct Reservation {Endpoint at{};uint64_t token=0,id=0,epoch=0,deadline=0;uint8_t mask=0;std::string name;};
    std::optional<Reservation>reservation_;
    bool lateReservations_=false;uint64_t reservationSequence_=0;
    uint64_t provisionalId_=0,provisionalHigh_=0,provisionalRoom_=0,provisionalEpoch_=0,provisionalUntil_=0;uint8_t provisionalMask_=0;
    bool epochFramed_=false;
    std::optional<MembershipChannel>membership_;
    std::deque<MembershipEpoch::Effect>membershipEffects_;
    struct Transfer {uint64_t transaction;MembershipJournal journal;};
    std::array<std::optional<Transfer>,4>journals_{};
public:
    // The roster a membership transition proposes: who owns each seat, and its names.
    // Nothing of it routes game data until PublishPending, after the transition's commit.
    struct PendingRoster {uint64_t id=0;uint8_t filled=0;std::array<uint8_t,4>owners{255,255,255,255};std::array<std::string,4>names{};int joining=-1,leaving=-1;};
private:
    std::optional<PendingRoster>pending_;
    std::vector<uint8_t> RosterBody(const PendingRoster&p)const{
        std::vector<uint8_t>b;Put64(b,p.id);b.push_back(p.filled);b.insert(b.end(),p.owners.begin(),p.owners.end());
        for(const auto&n:p.names){b.push_back(uint8_t(n.size()));b.insert(b.end(),n.begin(),n.end());}return b;
    }
    bool RosterValid(const PendingRoster&p)const{
        if(p.filled&~Full()||!(p.filled&1)||p.owners[0]!=0)return false;
        for(unsigned i=0;i<4;++i){
            if(p.filled&(1u<<i)){if(i>=capacity_||p.owners[i]>=capacity_||!(p.filled&(1u<<p.owners[i]))||p.owners[p.owners[i]]!=p.owners[i]||!NameOk(p.names[i]))return false;}
            else if(p.owners[i]!=255||!p.names[i].empty())return false;
        }
        for(unsigned i=0;i<capacity_;++i){unsigned count=0;for(unsigned j=0;j<capacity_;++j)count+=bool((p.filled&(1u<<j))&&p.owners[j]==i);if(count>2)return false;}
        return true;
    }
    void ReceiveRoster(std::span<const uint8_t>b,uint64_t room){
        // A guest already in the room: the roster the host's next transition proposes
        if(!ready_||!membership_||room!=room_||b.size()<13)return Reject();
        PendingRoster p;p.id=Get64(b.data());p.filled=b[8];std::copy_n(b.begin()+9,4,p.owners.begin());size_t at=13;
        for(unsigned i=0;i<4;++i){if(at>=b.size())return Reject();size_t n=b[at++];if(n>63||at+n>b.size())return Reject();p.names[i].assign(reinterpret_cast<const char*>(b.data()+at),n);at+=n;}
        if(at!=b.size()||!p.id||!RosterValid(p))return Reject();
        // (this PC keeps its own seats)
        for(unsigned i=0;i<capacity_;++i)if((mask_&(1u<<i))&&p.owners[i]!=primary_)return Reject();
        if(pending_&&pending_->id==p.id&&(pending_->filled!=p.filled||pending_->owners!=p.owners))return Reject();
        pending_=p;
    }
    void SendPendingTo(unsigned seat){
        if(!pending_||!peers_[seat].used)return;
        const auto&p=*pending_;const auto&peer=peers_[seat];
        if(int(seat)==p.joining){
            // (the candidate: an offer of the seats it will have, with the proposed roster)
            std::vector<uint8_t>b={uint8_t(capacity_),peer.mask,p.filled};b.insert(b.end(),p.owners.begin(),p.owners.end());
            b.push_back(uint8_t(profile_.size()>>8));b.push_back(uint8_t(profile_.size()));b.insert(b.end(),profile_.begin(),profile_.end());
            for(const auto&name:p.names){b.push_back(uint8_t(name.size()));b.insert(b.end(),name.begin(),name.end());}
            Emit(Wire('O',0,uint8_t(seat),room_,peer.token,b),peer.at,seat);return;
        }
        if(int(seat)==p.leaving)return;
        auto body=RosterBody(p);Emit(Wire('R',0,uint8_t(seat),room_,peer.token,body),peer.at,seat);
    }
public:
    // Host: the reserved candidate becomes the next transition's joining PC (pinned for controls and its journal);
    // the active roster is unchanged
    std::optional<PendingRoster> BeginJoin(uint64_t id){
        if(!host_||!ready_||!membership_||membership_->Failed()||pending_||!reservation_||!id)return {};
        const auto r=*reservation_;const unsigned seat=std::countr_zero(unsigned(r.mask));
        if(seat==0||seat>=capacity_||peers_[seat].used||(filled_&r.mask))return {};
        PendingRoster p;p.id=id;p.filled=uint8_t(filled_|r.mask);p.owners=owners_;p.names=names_;p.joining=int(seat);
        for(unsigned i=0;i<capacity_;++i)if(r.mask&(1u<<i)){p.owners[i]=uint8_t(seat);p.names[i]=i==seat?r.name:r.name.substr(0,61)+" 2";}
        if(!RosterValid(p))return {};
        membership_->Unpin(seat);if(!membership_->PinPeer(seat,r.at,r.token))return {};
        peers_[seat]=Peer{true,r.at,r.token,r.mask,0};reservation_.reset();pending_=p;return p;
    }
    // Host: a PC that left (or is leaving) is the next transition's leaver; its seats go back to the host's empty ones
    std::optional<PendingRoster> BeginLeave(unsigned peer,uint64_t id){
        if(!host_||!ready_||!membership_||membership_->Failed()||pending_||peer==0||peer>=capacity_||!peers_[peer].used||!id)return {};
        PendingRoster p;p.id=id;p.filled=uint8_t(filled_&~peers_[peer].mask);p.owners=owners_;p.names=names_;p.leaving=int(peer);
        for(unsigned i=0;i<capacity_;++i)if(owners_[i]==peer){p.owners[i]=255;p.names[i].clear();}
        if(!RosterValid(p))return {};
        pending_=p;return p;
    }
    // Host: the proposed roster to every PC that stays or joins (resent with each Prepare)
    void SendPending(){if(host_&&pending_)for(unsigned i=1;i<capacity_;++i)SendPendingTo(i);}
    const std::optional<PendingRoster>& Pending()const{return pending_;}
    // The pending roster as the membership channel stages it (the PCs present, each seat's owner)
    std::optional<MembershipRoster> PendingMembership(uint64_t id)const{
        if(!pending_||pending_->id!=id)return {};
        MembershipRoster r;r.capacity=capacity_;
        for(unsigned i=0;i<capacity_;++i)if(pending_->filled&(1u<<i)){r.owners[i]=pending_->owners[i];r.members|=uint8_t(1u<<pending_->owners[i]);}
        return r;
    }
    // After the transition's commit, once this PC has its checkpoint and before its new SDK session: the roster routes
    bool PublishPending(){
        if(!pending_)return false;
        const auto p=*pending_;pending_.reset();
        filled_=p.filled;owners_=p.owners;names_=p.names;
        if(host_){
            if(p.leaving>0){peers_[p.leaving]=Peer{};membership_->Unpin(unsigned(p.leaving));}
            for(unsigned i=1;i<capacity_;++i)if(peers_[i].used)peers_[i].ack=filled_;
        }
        else if(!ready_)ready_=true; // (the candidate: in the room from now on)
        return true;
    }
    // A transition abandoned before its commit: the candidate's place freed, the active roster as it was
    void CancelPending(){
        if(!pending_)return;
        if(host_&&pending_->joining>0){membership_->Unpin(unsigned(pending_->joining));peers_[pending_->joining]=Peer{};}
        pending_.reset();
    }
    // A candidate holding the host's offer of its seats: its channel for the transition that admits it (it is not a
    // member yet; the active roster is the offer's without its own seats)
    bool AttachCandidate(){
        if(host_||ready_||membership_||!mask_||!provisionalEpoch_||!room_)return false;
        PendingRoster p;p.id=0;p.filled=filled_;p.owners=owners_;p.names=names_;p.joining=int(primary_);
        MembershipRoster active;active.capacity=capacity_;
        for(unsigned i=0;i<capacity_;++i)if((filled_&(1u<<i))&&owners_[i]!=primary_){active.owners[i]=owners_[i];active.members|=uint8_t(1u<<owners_[i]);}
        MembershipChannel channel;if(!channel.Reset(primary_,room_,provisionalEpoch_,active)||!channel.PinPeer(0,hostAt_,token_))return false;
        membership_=std::move(channel);epochFramed_=true;pending_=p;return true;
    }
    // (the candidate's own pending roster takes the transaction's id once its Prepare arrives)
    void AdoptPendingId(uint64_t id){if(pending_&&!host_&&!pending_->id)pending_->id=id;}
    // The PC whose SDK actors were this handle's (for a disconnection), or 255
    unsigned OwnerOfSeat(unsigned seat)const{return seat<capacity_?RoutingOwner(seat):255;}
    bool Candidate()const{return !host_&&!ready_&&membership_.has_value();}
private:
    bool JournalActive(unsigned peer)const{
        return peer<4&&journals_[peer]&&membership_&&!membership_->Failed()&&membership_->Epoch().State()==MembershipEpoch::Stage::Preparing&&journals_[peer]->transaction==membership_->Epoch().Current().id;
    }
    static constexpr size_t header=24;
    static void Put64(std::vector<uint8_t>&b,uint64_t value){for(int i=7;i>=0;--i)b.push_back(uint8_t(value>>(8*i)));}
    static uint64_t Get64(const uint8_t*b){uint64_t n=0;for(int i=0;i<8;++i)n=(n<<8)|b[i];return n;}
    static bool NameOk(const std::string&n){return !n.empty()&&n.size()<=63&&std::all_of(n.begin(),n.end(),[](unsigned char c){return c>=32&&c!=127;});}
    uint8_t Full()const{return uint8_t((1u<<capacity_)-1);}
    int Physical(const Endpoint&at)const{for(unsigned i=1;i<capacity_;++i)if(peers_[i].used&&Same(peers_[i].at,at))return int(i);return -1;}
    std::vector<uint8_t> Wire(char kind,uint8_t source,uint8_t target,uint64_t room,uint64_t token,std::span<const uint8_t>body)const{
        std::vector<uint8_t>b={'R','S','T','1',1,uint8_t(kind),source,target};Put64(b,room);Put64(b,token);b.insert(b.end(),body.begin(),body.end());return b;
    }
    bool Emit(std::span<const uint8_t>b,const Endpoint&at,unsigned physical){if(!udp_.Send(b,at))return false;++sentTo[physical];return true;}
    void Offer(unsigned seat){
        const auto&p=peers_[seat];std::vector<uint8_t>b={uint8_t(capacity_),p.mask,filled_};b.insert(b.end(),owners_.begin(),owners_.end());
        b.push_back(uint8_t(profile_.size()>>8));b.push_back(uint8_t(profile_.size()));b.insert(b.end(),profile_.begin(),profile_.end());
        for(const auto&name:names_){b.push_back(uint8_t(name.size()));b.insert(b.end(),name.begin(),name.end());}
        Emit(Wire('O',0,uint8_t(seat),room_,p.token,b),p.at,seat);
    }
    void Ack(){std::array<uint8_t,1>b{filled_};Emit(Wire('A',primary_,0,room_,token_,b),hostAt_,0);}
    void Refuse(const Endpoint&at,uint64_t token,uint8_t reason,uint64_t now){
        if(std::getenv("STAR_DEBUG"))std::fprintf(stderr,"star: refuse reason %u (ready %d late %d filled %u reservation %d pending %d)\n",unsigned(reason),int(ready_),int(lateReservations_),unsigned(filled_),int(reservation_.has_value()),int(pending_.has_value()));
        const uint64_t key=(uint64_t(at.sin_addr.s_addr)<<16)|at.sin_port;auto found=refusedAt_.find(key);
        if(found!=refusedAt_.end()&&now-found->second<1000)return;
        if(refusedAt_.size()>=512)refusedAt_.clear();
        refusedAt_[key]=now;
        std::array<uint8_t,1>b{reason};udp_.Send(Wire('X',0,255,room_,token,b),at);
    }
    static uint64_t Deadline(uint64_t now){return now>UINT64_MAX-30000?UINT64_MAX:now+30000;}
    void ReservedOffer(){
        if(!reservation_)return;
        const auto&r=*reservation_;std::vector<uint8_t>b{uint8_t(capacity_),r.mask};Put64(b,r.id);Put64(b,r.epoch);
        b.push_back(uint8_t(profile_.size()>>8));b.push_back(uint8_t(profile_.size()));b.insert(b.end(),profile_.begin(),profile_.end());
        udp_.Send(Wire('P',0,255,room_,r.token,b),r.at);
    }
    void ReserveLate(const Endpoint&at,uint64_t token,unsigned count,const std::string&name,uint64_t now){
        if(!lateReservations_||!membership_||membership_->Failed()){Refuse(at,token,1,now);return;}
        if(reservation_&&(now>=reservation_->deadline||!membership_||membership_->Failed()||membership_->Epoch().Epoch()!=reservation_->epoch))reservation_.reset();
        if(reservation_){const auto&r=*reservation_;if(Same(r.at,at)){
            if(r.token!=token||std::popcount(unsigned(r.mask))!=int(count)||r.name!=name){Refuse(at,token,3,now);return;}
            ReservedOffer();return;
        }Refuse(at,token,1,now);return;}
        if(!membership_->MayAdvance())return; // (a transition under way: the joiner's next Hello is answered after it)
        unsigned seat=capacity_;for(unsigned i=1;i+count<=capacity_;++i){bool free=true;for(unsigned j=0;j<count;++j)free=free&&!(filled_&(1u<<(i+j)));if(free){seat=i;break;}}
        if(seat==capacity_||reservationSequence_==UINT64_MAX){Refuse(at,token,1,now);return;}
        reservation_=Reservation{at,token,++reservationSequence_,membership_->Epoch().Epoch(),Deadline(now),uint8_t(((1u<<count)-1)<<seat),name};ReservedOffer();
    }
    void ReceiveProvisional(std::span<const uint8_t>b,uint64_t room,uint64_t now){
        if(ready_||mask_||!room||b.size()<20||b[0]!=capacity_)return Reject();
        const unsigned mask=b[1],primary=mask?std::countr_zero(mask):0;
        if(!mask||(mask&1)||(mask&~Full())||std::popcount(mask)!=int(requested_)||mask!=(((1u<<requested_)-1)<<primary))return Reject();
        const uint64_t id=Get64(b.data()+2),epoch=Get64(b.data()+10);unsigned p=(unsigned(b[18])<<8)|b[19];
        if(!id||!epoch||p!=profile_.size()||b.size()!=20+p||!std::equal(profile_.begin(),profile_.end(),b.begin()+20))return Reject();
        if((provisionalRoom_&&room!=provisionalRoom_)||id<provisionalHigh_)return Reject();
        if(id==provisionalHigh_){if(provisionalId_!=id||provisionalMask_!=mask||provisionalEpoch_!=epoch)return Reject();return;}
        provisionalRoom_=room;provisionalId_=provisionalHigh_=id;provisionalEpoch_=epoch;provisionalMask_=uint8_t(mask);provisionalUntil_=Deadline(now);
    }
    void Hello(const Endpoint&at,uint64_t token,std::span<const uint8_t>b,uint64_t now){
        if(b.size()<4||!token)return Reject();
        unsigned count=b[0],n=b[1],p=(unsigned(b[2])<<8)|b[3];
        if((count!=1&&count!=2)||n>63||p>16384||b.size()!=4+n+p)return Reject();
        std::string name(reinterpret_cast<const char*>(b.data()+4),n);if(!NameOk(name))return Reject();
        if(p!=profile_.size()||!std::equal(profile_.begin(),profile_.end(),b.begin()+4+n)){Refuse(at,token,2,now);return;}
        int known=Physical(at);
        if(known>=0){const auto&old=peers_[known];if(old.token!=token||std::popcount(old.mask)!=int(count)){Refuse(at,token,3,now);return;}
            if(pending_&&pending_->joining==known){SendPendingTo(unsigned(known));return;}
            Offer(unsigned(known));return;}
        if(ready_){ReserveLate(at,token,count,name,now);return;}
        unsigned seat=capacity_;for(unsigned i=1;i+count<=capacity_;++i){bool free=true;for(unsigned j=0;j<count;++j)free=free&&!(filled_&(1u<<(i+j)));if(free){seat=i;break;}}
        if(seat==capacity_||ready_){Refuse(at,token,1,now);return;}
        auto&peer=peers_[seat];peer.used=true;peer.at=at;peer.token=token;peer.mask=uint8_t(((1u<<count)-1)<<seat);
        for(unsigned j=0;j<count;++j){owners_[seat+j]=uint8_t(seat);names_[seat+j]=j?name.substr(0,61)+" 2":name;}
        filled_|=peer.mask;Offer(seat);
    }
    void ReceiveOffer(std::span<const uint8_t>b,uint8_t target,uint64_t room){
        if(b.size()<13||!room||b[0]!=capacity_)return Reject();
        uint8_t mask=b[1],filled=b[2];
        if(!mask||(mask&1)||(mask&~Full())||(filled&~Full())||!(filled&1)||(mask&filled)!=mask||std::popcount(mask)!=int(requested_))return Reject();
        const unsigned primary=std::countr_zero(unsigned(mask));if(target!=primary)return Reject();
        std::array<uint8_t,4>owners{};std::copy_n(b.begin()+3,4,owners.begin());
        unsigned p=(unsigned(b[7])<<8)|b[8];if(p!=profile_.size()||b.size()<9+p+4||!std::equal(profile_.begin(),profile_.end(),b.begin()+9))return Reject();
        std::array<std::string,4>names{};size_t at=9+p;
        for(unsigned i=0;i<4;++i){if(at>=b.size())return Reject();size_t n=b[at++];if(n>63||at+n>b.size())return Reject();names[i].assign(reinterpret_cast<const char*>(b.data()+at),n);at+=n;
            if(filled&(1u<<i)){if(i>=capacity_||owners[i]>i||owners[i]>=capacity_||!(filled&(1u<<owners[i]))||owners[owners[i]]!=owners[i]||!NameOk(names[i]))return Reject();}
            else if(owners[i]!=255||!names[i].empty())return Reject();
            if((mask&(1u<<i))&&owners[i]!=primary)return Reject();
        }
        if(at!=b.size()||owners[0]!=0)return Reject();
        for(unsigned i=0;i<capacity_;++i){unsigned count=0;for(unsigned j=0;j<capacity_;++j)count+=bool((filled&(1u<<j))&&owners[j]==i);if(count>2)return Reject();}
        if(mask_&&(mask_!=mask||room_!=room||(filled_&filled)!=filled_))return Reject();
        for(unsigned i=0;i<capacity_;++i)if((filled_&(1u<<i))&&owners_[i]!=owners[i])return Reject();
        primary_=uint8_t(primary);mask_=mask;filled_=filled;owners_=owners;names_=std::move(names);room_=room;Ack();
    }
    void Reject(){++rejected;}
    void Queue(unsigned source,std::span<const uint8_t>b){if(incoming.size()>=512){Reject();return;}incoming.push_back({source,std::vector<uint8_t>(b.begin(),b.end()),epochFramed_?membership_->GameEpoch():0});++logicalFrom[source];}
public:
    struct Packet {unsigned source;std::vector<uint8_t>bytes;uint64_t epoch=0;};std::deque<Packet>incoming;
    std::array<uint64_t,4>sentTo{},receivedFrom{},logicalFrom{};uint64_t relayed=0,rejected=0;
    bool Open(bool host,uint16_t port,const std::string&address,uint64_t token,unsigned capacity,unsigned requested,const std::string&name,std::span<const uint8_t>profile,std::string&error,BindScope scope=BindScope::Any,unsigned initial=0){
        if(udp_){error="The seat transport is already open";return false;}
        if(!token||capacity<2||capacity>4||(requested!=1&&requested!=2)||!NameOk(name)||profile.empty()||profile.size()>16384){error="Invalid seat transport configuration";return false;}
        initial_=initial?initial:capacity;
        if(initial_<2||initial_>capacity||(host&&initial_<=requested)){error="Invalid initial player threshold";return false;}
        host_=host;capacity_=capacity;requested_=requested;token_=token;name_=name;profile_.assign(profile.begin(),profile.end());
        if(!udp_.Open(host?port:0,error,scope))return false;
        if(host){
            room_=token;primary_=0;mask_=filled_=uint8_t((1u<<requested)-1);
            for(unsigned i=0;i<requested;++i){owners_[i]=0;names_[i]=i?name.substr(0,61)+" 2":name;}
            ready_=filled_==Full();
        }
        else if(!udp_.Resolve(address,port,hostAt_,error)){udp_.Close();return false;}
        return true;
    }
    unsigned Owner(unsigned seat)const{return seat<owners_.size()&&seat<capacity_?owners_[seat]:255;}
    unsigned RoutingOwner(unsigned seat)const{if(seat>=capacity_)return 255;return owners_[seat]==255?0:owners_[seat];}
    bool SdkLocal(unsigned seat)const{return RoutingOwner(seat)==primary_;}
    bool Owns(unsigned seat)const{return seat<owners_.size()&&seat<capacity_&&bool(mask_&(1u<<seat));}
    bool Local(Endpoint&at)const{return udp_.Local(at);}
    bool Ready()const{return ready_;}unsigned Primary()const{return primary_;}unsigned Capacity()const{return capacity_;}uint8_t Mask()const{return mask_;}uint8_t Filled()const{return filled_;}
    uint64_t Room()const{return room_;}bool IsHost()const{return host_;}const std::array<std::string,4>&Names()const{return names_;}const std::string&Refusal()const{return refusal_;}
    // Explicit attachment: actual gameplay must wire checkpoint/SDK publication first.
    bool AttachMembership(){
        if(!ready_||membership_)return false;
        MembershipRoster roster;roster.capacity=capacity_;roster.owners=owners_;
        for(unsigned i=0;i<capacity_;++i)if(owners_[i]<4)roster.members|=uint8_t(1u<<owners_[i]);
        MembershipChannel channel;if(!channel.Reset(primary_,room_,1,roster))return false;
        if(host_){for(unsigned i=1;i<capacity_;++i)if(peers_[i].used&&!channel.PinPeer(i,peers_[i].at,peers_[i].token))return false;}
        else if(!channel.PinPeer(0,hostAt_,token_))return false;
        membership_=std::move(channel);return true;
    }
    MembershipChannel* Membership(){return membership_?&*membership_:nullptr;}
    bool EnableEpochFraming(){
        if(epochFramed_||!membership_||!membership_->MayAdvance())return false;
        incoming.clear();epochFramed_=true;return true;
    }
    bool EpochFramed()const{return epochFramed_;}
    bool ValidQueued(const Packet&p)const{return !epochFramed_||(membership_&&membership_->AcceptGame(p.epoch,p.source));}
    bool SendMembership(unsigned peer,const MembershipEpoch::Effect&e){return membership_&&membership_->Send(udp_,peer,e);}
    std::optional<MembershipEpoch::Effect> TakeMembership(){
        if(membershipEffects_.empty())return {};
        auto e=membershipEffects_.front();membershipEffects_.pop_front();return e;
    }
    bool EnableLateReservations(){if(!host_||!ready_||!epochFramed_||!membership_||!membership_->MayAdvance())return false;lateReservations_=true;return true;}
    uint64_t ReservationId()const{return reservation_?reservation_->id:0;}
    uint8_t ReservationMask()const{return reservation_?reservation_->mask:0;}
    uint64_t ReservationDeadline()const{return reservation_?reservation_->deadline:0;}
    void CancelReservation(){reservation_.reset();}
    uint64_t ProvisionalId()const{return provisionalId_;}
    uint8_t ProvisionalMask()const{return provisionalMask_;}
    uint64_t ProvisionalDeadline()const{return provisionalUntil_;}
    bool StartJournal(unsigned peer,const ConfirmedInputStaging::Identity&identity,unsigned lanes,uint64_t now,std::span<const uint16_t>confirmed={}){
        if(!membership_||peer>=4||journals_[peer])return false;
        MembershipJournal journal;if(!membership_->OpenJournal(journal,peer,identity,lanes,now,confirmed))return false;
        journals_[peer]=Transfer{membership_->Epoch().Current().id,std::move(journal)};return true;
    }
    const MembershipJournal* Journal(unsigned peer)const{return JournalActive(peer)?&journals_[peer]->journal:nullptr;}
    void CancelJournals(){for(auto&j:journals_)j.reset();}
    void Tick(uint64_t now){
        if(reservation_&&(now>=reservation_->deadline||!membership_||membership_->Failed()||membership_->Epoch().Epoch()!=reservation_->epoch))reservation_.reset();
        if(provisionalUntil_&&now>=provisionalUntil_){provisionalId_=provisionalUntil_=0;provisionalMask_=0;}

        for(unsigned peer=0;peer<4;++peer)if(journals_[peer]){
            if(!JournalActive(peer)){journals_[peer].reset();continue;}
            auto&journal=journals_[peer]->journal;if(!journal.Poll(now))continue;
            auto request=journal.Request(now);if(!request.empty())udp_.Send(request,hostAt_);
        }
        if(now<next_)return;
        next_=now+250;
        if(host_&&reservation_)ReservedOffer();
        if(host_){for(unsigned i=1;i<capacity_;++i)if(peers_[i].used){
            if(pending_&&(int(i)==pending_->joining||int(i)==pending_->leaving)){SendPendingTo(i);continue;}
            Offer(i);if(ready_){std::array<uint8_t,1>b{filled_};Emit(Wire('B',0,uint8_t(i),room_,peers_[i].token,b),peers_[i].at,i);}
            if(pending_)SendPendingTo(i);
        }}
        else if(!mask_){std::vector<uint8_t>b={uint8_t(requested_),uint8_t(name_.size()),uint8_t(profile_.size()>>8),uint8_t(profile_.size())};b.insert(b.end(),name_.begin(),name_.end());b.insert(b.end(),profile_.begin(),profile_.end());Emit(Wire('H',255,0,0,token_,b),hostAt_,0);}
        else Ack();
    }
    void Pump(uint64_t now){
        for(unsigned batch=0;batch<256;++batch){std::array<uint8_t,20000>b{};Endpoint from{};auto got=udp_.Receive(b,from);
            if(got.kind==ReceiveKind::PeerUnreachable)continue;
            if(got.kind==ReceiveKind::WouldBlock)break;
            if(got.kind!=ReceiveKind::Packet){Reject();break;}
            auto bytes=std::span<const uint8_t>(b.data(),got.size);
            if(bytes.size()>=4&&!std::memcmp(b.data(),"RME1",4)){
                if(!membership_){Reject();continue;}
                auto effect=membership_->Receive(from,bytes,now);
                if(!effect.accepted){Reject();continue;}
                if(membershipEffects_.size()>=64){membership_->Fail();membershipEffects_.clear();MembershipEpoch::Effect failure;failure.accepted=failure.failed=true;membershipEffects_.push_back(failure);}
                else membershipEffects_.push_back(effect);
                continue;
            }
            if(bytes.size()>=4&&!std::memcmp(b.data(),"RMJ1",4)){
                if(bytes.size()<8){Reject();continue;}
                unsigned peer=host_?unsigned(b[6]):0;
                if(!JournalActive(peer)){Reject();continue;}
                auto response=journals_[peer]->journal.Receive(from,bytes,now);
                if(!response.empty())udp_.Send(response,from);
                continue;
            }
            if(bytes.size()<header||std::memcmp(b.data(),"RST1",4)||b[4]!=1){Reject();continue;}
            char kind=char(b[5]);unsigned source=b[6],target=b[7];uint64_t room=Get64(b.data()+8),token=Get64(b.data()+16);auto body=bytes.subspan(header);
            if(host_){
                if(kind=='H'&&source==255&&target==0&&!room){Hello(from,token,body,now);continue;}
                int physical=Physical(from);if(physical<0||source!=unsigned(physical)||token!=peers_[physical].token||room!=room_){Reject();continue;}++receivedFrom[physical];
                if(kind=='A'&&target==0&&body.size()==1&&body[0]==filled_){peers_[physical].ack=filled_;bool all=std::popcount(unsigned(filled_))>=int(initial_);for(unsigned i=1;i<capacity_;++i)if(peers_[i].used)all=all&&peers_[i].ack==filled_;if(all)ready_=true;continue;}
                if(kind!='G'||body.empty()||body.size()>(epochFramed_?4108u:4096u)||target>=capacity_||!(filled_&(1u<<target))||owners_[target]==source){Reject();continue;}
                auto payload=body;
                if(epochFramed_){payload=membership_wire::GamePayload(body,membership_->GameEpoch());if(payload.empty()||!membership_->AcceptGame(membership_->GameEpoch(),source)||!membership_->AcceptGame(membership_->GameEpoch(),owners_[target])){Reject();continue;}}
                if(owners_[target]==primary_)Queue(source,payload);
                else{unsigned owner=owners_[target];if(!peers_[owner].used){Reject();continue;}auto forward=Wire('G',uint8_t(source),uint8_t(target),room_,peers_[owner].token,body);if(Emit(forward,peers_[owner].at,owner))++relayed;}
            }else{
                if(!Same(from,hostAt_)||token!=token_){Reject();continue;}++receivedFrom[0];
                if(kind=='X'&&source==0&&target==255&&body.size()==1){if((mask_&&room!=room_)||body[0]<1||body[0]>3){Reject();continue;}refusal_=body[0]==1?"The match is full":body[0]==2?"The game, ROMs or settings do not match":"The seat request changed";continue;}
                if(kind=='P'&&source==0&&target==255){ReceiveProvisional(body,room,now);continue;}
                if(kind=='O'&&source==0){if(!membership_)ReceiveOffer(body,uint8_t(target),room);continue;}
                if(kind=='R'&&source==0&&target==primary_){ReceiveRoster(body,room);continue;}
                if(!mask_||room!=room_||target>=capacity_||owners_[target]!=primary_){Reject();continue;}
                if(kind=='B'&&source==0&&body.size()==1&&body[0]==filled_&&std::popcount(unsigned(filled_))>=int(initial_)){ready_=true;continue;}
                if(kind!='G'||source>=capacity_||owners_[source]!=source||source==primary_||body.empty()||body.size()>(epochFramed_?4108u:4096u)){Reject();continue;}
                auto payload=body;
                if(epochFramed_){payload=membership_wire::GamePayload(body,membership_->GameEpoch());if(payload.empty()||!membership_->AcceptGame(membership_->GameEpoch(),source)){Reject();continue;}}
                Queue(source,payload);
            }
        }
    }
    bool Send(unsigned target,std::span<const uint8_t>body){
        if(!ready_||body.empty()||body.size()>4096||target>=capacity_||!(filled_&(1u<<target))||owners_[target]==primary_)return false;
        std::vector<uint8_t>framed;
        if(epochFramed_){
            const auto epoch=membership_->GameEpoch();
            if(!membership_->AcceptGame(epoch,primary_)||!membership_->AcceptGame(epoch,owners_[target]))return false;
            framed=membership_wire::Game(epoch,body);if(framed.empty())return false;body=framed;
        }
        unsigned owner=owners_[target];return host_?Emit(Wire('G',primary_,uint8_t(target),room_,peers_[owner].token,body),peers_[owner].at,owner):Emit(Wire('G',primary_,uint8_t(target),room_,token_,body),hostAt_,0);
    }
};
struct StarAdapter {
    inline static StarRoom* active=nullptr;inline static std::vector<GekkoNetResult*>handed;
    inline static constexpr char names[4][6]={"seat0","seat1","seat2","seat3"};
    static GekkoNetAddress Address(unsigned seat){if(seat>=4)return {};return {const_cast<char*>(names[seat]),5};}
    static void SendData(GekkoNetAddress*at,const char*data,int length){
        if(!active||!at||!at->data||at->size!=5||length<=0||!data)return;
        auto p=static_cast<const char*>(at->data);if(std::memcmp(p,"seat",4)||p[4]<'0'||p[4]>'3')return;
        active->Send(unsigned(p[4]-'0'),std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data),size_t(length)));
    }
    static GekkoNetResult**ReceiveData(int*length){
        handed.clear();if(active)while(!active->incoming.empty()){
            auto p=std::move(active->incoming.front());active->incoming.pop_front();if(!active->ValidQueued(p))continue;auto*result=static_cast<GekkoNetResult*>(std::calloc(1,sizeof(GekkoNetResult)));if(!result)continue;
            result->addr.size=5;result->addr.data=std::malloc(5);result->data_len=unsigned(p.bytes.size());result->data=std::malloc(p.bytes.size());
            if(!result->addr.data||!result->data){std::free(result->addr.data);std::free(result->data);std::free(result);continue;}
            std::memcpy(result->addr.data,names[p.source],5);std::memcpy(result->data,p.bytes.data(),p.bytes.size());handed.push_back(result);
        }
        *length=int(handed.size());return handed.data();
    }
    static void FreeData(void*p){std::free(p);}inline static GekkoNetAdapter callbacks{SendData,ReceiveData,FreeData};
};
}





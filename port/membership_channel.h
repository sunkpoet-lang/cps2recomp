// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "membership_wire.h"
#include "membership_journal.h"
#include "udp_transport.h"
namespace recomp::net {
struct MembershipRoster {
 unsigned capacity=0;uint8_t members=0;
 std::array<uint8_t,4> owners{255,255,255,255};
 bool Valid()const{
  if(capacity<2||capacity>4||!(members&1)||(members&~15u)||owners[0]!=0)return false;
  std::array<unsigned,4>counts{};
  for(unsigned i=0;i<4;++i){auto p=owners[i];if(i>=capacity){if(p!=255)return false;continue;}if(p==255)continue;if(p>=4||!(members&(1u<<p))||++counts[p]>2)return false;}
  for(unsigned p=0;p<4;++p)if(bool(members&(1u<<p))!=bool(counts[p]))return false;
  return true;
 }
 unsigned RoutingOwner(unsigned seat)const{return seat<capacity?(owners[seat]==255?0:owners[seat]):255;}
 bool Extends(const MembershipRoster&old)const{
  if(!Valid()||!old.Valid()||capacity!=old.capacity||std::popcount(unsigned(members^old.members))!=1)return false;
  for(unsigned i=0;i<capacity;++i){auto p=old.owners[i],n=owners[i];if(p==n)continue;
   if(p!=255&&(members&(1u<<p)))return false;
   if(n!=255&&(old.members&(1u<<n)))return false;
  }
  return true;
 }
 bool operator==(const MembershipRoster&)const=default;
};
// Admission pins endpoints separately. Caller multiplexes datagrams and dispatches Effects.
class MembershipChannel {
 struct Pin {bool set=false;Endpoint endpoint{};uint64_t token=0;};
 std::array<Pin,4>pins_{};MembershipEpoch epoch_;MembershipRoster active_{};
 std::optional<MembershipRoster>pending_;std::optional<MembershipEpoch::Effect>apply_;
 unsigned self_=0;uint64_t room_=0;bool failed_=true;
 MembershipEpoch::Effect Handle(MembershipEpoch::Effect e){
  if(e.failed){failed_=true;return e;}
  if(e.resumeOld){pending_.reset();apply_.reset();}
  if(e.apply){if(!pending_){Fail();e.send.reset();e.failed=true;return e;}apply_=e;e.send.reset();}
  return e;
 }
public:
 bool Reset(unsigned self,uint64_t room,uint64_t epoch,const MembershipRoster&active){
  if(!room||!active.Valid()||!epoch_.Reset(self,epoch,active.members))return false;
  self_=self;room_=room;active_=active;pins_={};pending_.reset();apply_.reset();failed_=false;return true;
 }
 bool PinPeer(unsigned peer,const Endpoint&at,uint64_t token){
  if(failed_||peer>=4||peer==self_||!token||!at.sin_port||at.sin_family!=AF_INET||pins_[peer].set)return false;
  pins_[peer]={true,at,token};return true;
 }
 void Unpin(unsigned peer){if(peer<4&&peer!=self_)pins_[peer]={};}
 bool Pinned(unsigned peer)const{return peer<4&&pins_[peer].set;}
 const MembershipRoster& Active()const{return active_;}
 const MembershipEpoch& Epoch()const{return epoch_;}
 bool MayAdvance()const{return !failed_&&!apply_&&epoch_.MayAdvance();}
 bool Failed()const{return failed_;}
 uint64_t GameEpoch()const{return failed_||apply_?0:epoch_.Epoch();}
 bool AcceptGame(uint64_t epoch,unsigned source)const{return !failed_&&!apply_&&source<4&&(active_.members&(1u<<source))&&epoch_.AcceptGame(epoch);}
 void Fail(){failed_=true;pending_.reset();apply_.reset();}
 bool Stage(const MembershipRoster&next){
  if(failed_||apply_||epoch_.State()!=MembershipEpoch::Stage::Preparing||!next.Extends(active_)||next.members!=epoch_.Current().newMembers)return false;
  if(pending_&&*pending_!=next)return false;
  pending_=next;return true;
 }
 bool OpenJournal(MembershipJournal&journal,unsigned peer,const ConfirmedInputStaging::Identity&identity,unsigned lanes,uint64_t now,std::span<const uint16_t>confirmed={})const{
  if(failed_||apply_||!pending_||epoch_.State()!=MembershipEpoch::Stage::Preparing||peer>=4||!pins_[peer].set)return false;
  const auto&p=epoch_.Current();const bool host=self_==0;
  if((!host&&peer!=0)||!(p.newMembers&(1u<<(host?peer:self_))))return false;
  const auto&pin=pins_[peer];MembershipJournal::Contract c{0,host?peer:self_,lanes,room_,pin.token,p.id,p.oldEpoch,p.frame,identity};
  return journal.Open(host,pin.endpoint,c,now,confirmed);
 }
 MembershipEpoch::Effect Begin(uint8_t next,uint64_t frame,uint64_t hash,uint64_t now){return failed_?MembershipEpoch::Effect{}:Handle(epoch_.Begin(next,frame,hash,now));}
 MembershipEpoch::Effect Verify(uint64_t frame,uint64_t hash,uint64_t now){return failed_||!pending_?MembershipEpoch::Effect{}:Handle(epoch_.VerifyLocal(frame,hash,now));}
 MembershipEpoch::Effect Tick(uint64_t now){return failed_||apply_?MembershipEpoch::Effect{}:Handle(epoch_.Tick(now));}
 MembershipEpoch::Effect Abort(){return failed_||apply_?MembershipEpoch::Effect{}:Handle(epoch_.Abort());}
 MembershipEpoch::Effect Receive(const Endpoint&from,std::span<const uint8_t>bytes,uint64_t now){
  if(failed_||apply_)return {};
  for(unsigned peer=0;peer<4;++peer){auto&p=pins_[peer];if(!p.set||!Same(from,p.endpoint))continue;
   auto m=membership_wire::Decode(bytes,peer,self_,room_,p.token);if(!m)return {};
   return Handle(epoch_.Receive(peer,*m,now));
  }
  return {};
 }
 // Only after successful own checkpoint load + SDK rebuild. Failure is terminal.
 MembershipEpoch::Effect Publish(bool success){
  if(failed_||!apply_)return {};
  if(!success){Fail();MembershipEpoch::Effect e;e.failed=true;return e;}
  active_=*pending_;pending_.reset();auto e=*apply_;apply_.reset();e.apply=false;return e;
 }
 bool Send(Udp&socket,unsigned peer,const MembershipEpoch::Effect&e){
  if(failed_||apply_||!e.send||peer>=4||!pins_[peer].set)return false;
  auto&p=pins_[peer];auto bytes=membership_wire::Encode(self_,peer,room_,p.token,*e.send);
  return !bytes.empty()&&socket.Send(bytes,p.endpoint);
 }
};
}

// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "membership_wire.h"
#include "udp_transport.h"
namespace recomp::net {
// One immutable prefix, one admitted candidate. No machine-state bytes are accepted.
class MembershipJournal {
public:
 struct Contract {
  unsigned host=0,guest=0,lanes=0;uint64_t room=0,token=0,transaction=0,epoch=0,frames=0;
  ConfirmedInputStaging::Identity identity{};
 };
private:
 Contract c_{};Endpoint peer_{};bool host_=false,valid_=false,failed_=false;
 uint64_t deadline_=0,next_=0;ConfirmedInputStaging staging_;std::vector<uint16_t>confirmed_;
 static uint64_t Add(uint64_t a,uint64_t b){return a>UINT64_MAX-b?UINT64_MAX:a+b;}
 bool Alive(uint64_t now){if(!valid_||failed_)return false;if(now>=deadline_){failed_=true;return false;}return true;}
 std::vector<uint8_t> Header(unsigned op,uint64_t first)const{
  std::vector<uint8_t>b={'R','M','J','1',1,uint8_t(op),uint8_t(host_?c_.host:c_.guest),uint8_t(host_?c_.guest:c_.host)};
  for(auto n:{c_.room,c_.token,c_.transaction,c_.epoch,c_.frames})membership_wire::Put64(b,n);
  b.insert(b.end(),c_.identity.begin(),c_.identity.end());b.push_back(uint8_t(c_.lanes));membership_wire::Put64(b,first);return b;
 }
public:
 bool Open(bool host,const Endpoint&peer,const Contract&c,uint64_t now,std::span<const uint16_t>confirmed={}){
  if(valid_||c.host!=0||c.guest==0||c.guest>=4||c.lanes<2||c.lanes>4||!c.room||!c.token||!c.transaction||!c.epoch||!c.frames||c.frames>ConfirmedInputStaging::maxFrames||peer.sin_family!=AF_INET||!peer.sin_port)return false;
  const size_t count=size_t(c.frames)*c.lanes;
  if((host&&confirmed.size()!=count)||(!host&&!confirmed.empty()))return false;
  if(!host&&!staging_.Reset(c.identity,c.lanes,c.frames))return false;
  c_=c;peer_=peer;host_=host;deadline_=Add(now,5000);next_=now;failed_=false;valid_=true;
  if(host)confirmed_.assign(confirmed.begin(),confirmed.end());
  return true;
 }
 bool Failed()const{return failed_;}
 bool Poll(uint64_t now){return Complete()||Alive(now);}
 bool Complete()const{return valid_&&!host_&&!failed_&&staging_.Complete();}
 std::span<const uint16_t> Inputs()const{return Complete()?staging_.Inputs():std::span<const uint16_t>();}
 std::vector<uint8_t> Request(uint64_t now){
  if(host_||Complete()||!Alive(now)||now<next_)return {};
  next_=Add(now,100);return Header(1,staging_.NextMissing());
 }
 std::vector<uint8_t> Receive(const Endpoint&from,std::span<const uint8_t>b,uint64_t now){
  if(Complete())return {};
  if(!Alive(now)||!Same(from,peer_)||b.size()<89||!std::equal(b.begin(),b.begin()+4,"RMJ1")||b[4]!=1||b[5]!=(host_?1:2)||b[6]!=(host_?c_.guest:c_.host)||b[7]!=(host_?c_.host:c_.guest))return {};
  const std::array<uint64_t,5>fields{c_.room,c_.token,c_.transaction,c_.epoch,c_.frames};
  for(unsigned i=0;i<5;++i)if(membership_wire::Get64(b.data()+8+i*8)!=fields[i])return {};
  if(!std::equal(c_.identity.begin(),c_.identity.end(),b.begin()+48)||b[80]!=c_.lanes)return {};
  const uint64_t first=membership_wire::Get64(b.data()+81);
  if(first>=c_.frames||first%ConfirmedInputStaging::chunkFrames)return {};
  const size_t count=size_t(std::min<uint64_t>(ConfirmedInputStaging::chunkFrames,c_.frames-first))*c_.lanes;
  if(host_){
   if(b.size()!=89||now<next_)return {};
   next_=Add(now,10);auto out=Header(2,first);const size_t start=size_t(first)*c_.lanes;
   for(size_t i=0;i<count;++i){auto n=confirmed_[start+i];out.push_back(uint8_t(n>>8));out.push_back(uint8_t(n));}return out;
  }
  if(b.size()!=89+count*2)return {};
  if(!staging_.Add(c_.identity,first,b.subspan(89)))failed_=true;
  else next_=now; // (the next block at once; the 100 ms clock retries a lost one)
  return {};
 }
};
}

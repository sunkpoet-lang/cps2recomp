// SPDX-License-Identifier: GPL-3.0-only
// Membership control codec and bounded confirmed-input staging; no external state loads.
#pragma once
#include "membership_epoch.h"
#include <algorithm>
#include <array>
#include <span>
#include <vector>
namespace recomp::net {
namespace membership_wire {
inline void Put64(std::vector<uint8_t>&b,uint64_t n){for(int i=7;i>=0;--i)b.push_back(uint8_t(n>>(i*8)));}
inline uint64_t Get64(const uint8_t*p){uint64_t n=0;for(unsigned i=0;i<8;++i)n=(n<<8)|p[i];return n;}
inline bool ProposalValid(const MembershipEpoch::Proposal&p){
 return p.id&&p.oldEpoch&&p.oldEpoch!=UINT64_MAX&&p.nextEpoch==p.oldEpoch+1&&(p.oldMembers&1)&&(p.newMembers&1)&&!(p.oldMembers&~15u)&&!(p.newMembers&~15u)&&std::popcount(unsigned(p.oldMembers^p.newMembers))==1;
}
inline bool Authority(unsigned source,MembershipEpoch::Kind kind){
 using K=MembershipEpoch::Kind;
 return (kind==K::Prepare||kind==K::Commit||kind==K::Start||kind==K::Abort)?source==0:source!=0;
}
inline std::vector<uint8_t> Encode(unsigned source,unsigned target,uint64_t room,uint64_t token,const MembershipEpoch::Message&m){
 if(source>=4||target>=4||source==target||!room||!token||unsigned(m.kind)>6||!Authority(source,m.kind)||!ProposalValid(m.proposal))return {};
 std::vector<uint8_t>b={'R','M','E','1',1,uint8_t(m.kind),uint8_t(source),uint8_t(target)};
 Put64(b,room);Put64(b,token);const auto&p=m.proposal;
 for(auto n:{p.id,p.oldEpoch,p.nextEpoch,p.frame,p.fingerprint})Put64(b,n);
 b.push_back(p.oldMembers);b.push_back(p.newMembers);return b;
}
// Endpoint matching must precede this call. The token is routing isolation, not encryption.
inline std::optional<MembershipEpoch::Message> Decode(std::span<const uint8_t>b,unsigned source,unsigned target,uint64_t room,uint64_t token){
 if(b.size()!=66||source>=4||target>=4||source==target||!room||!token||!std::equal(b.begin(),b.begin()+4,"RME1")||b[4]!=1||b[5]>6||b[6]!=source||b[7]!=target||Get64(b.data()+8)!=room||Get64(b.data()+16)!=token)return {};
 auto kind=MembershipEpoch::Kind(b[5]);if(!Authority(source,kind))return {};
 MembershipEpoch::Proposal p{Get64(b.data()+24),Get64(b.data()+32),Get64(b.data()+40),Get64(b.data()+48),Get64(b.data()+56),b[64],b[65]};
 if(!ProposalValid(p))return {};
 return MembershipEpoch::Message{kind,p};
}
inline std::vector<uint8_t> Game(uint64_t epoch,std::span<const uint8_t>payload){
 if(!epoch||payload.empty()||payload.size()>4096)return {};
 std::vector<uint8_t>b={'R','M','G','1'};Put64(b,epoch);b.insert(b.end(),payload.begin(),payload.end());return b;
}
inline std::span<const uint8_t> GamePayload(std::span<const uint8_t>b,uint64_t epoch){
 if(!epoch||b.size()<13||b.size()>4108||!std::equal(b.begin(),b.begin()+4,"RMG1")||Get64(b.data()+4)!=epoch)return {};
 return b.subspan(12);
}
}
class ConfirmedInputStaging {
public:
 using Identity=std::array<uint8_t,32>;
 static constexpr uint64_t maxFrames=216000;
 static constexpr unsigned chunkFrames=120;
private:
 Identity identity_{};unsigned lanes_=0;uint64_t frames_=0,received_=0;
 std::vector<uint16_t>inputs_;std::vector<uint8_t>seen_;
public:
 bool Reset(const Identity&identity,unsigned lanes,uint64_t frames){
  if(lanes<2||lanes>4||!frames||frames>maxFrames)return false;
  identity_=identity;lanes_=lanes;frames_=frames;received_=0;inputs_.assign(size_t(frames)*lanes,0);seen_.assign(size_t(frames),0);return true;
 }
 bool Add(const Identity&identity,uint64_t first,std::span<const uint8_t>bytes){
  if(!lanes_||identity!=identity_||bytes.empty()||bytes.size()%(lanes_*2))return false;
  const auto count=bytes.size()/(lanes_*2);
  if(count>chunkFrames||first>=frames_||count>frames_-first)return false;
  // Preflight every duplicate before applying any new frame from this chunk.
  for(size_t i=0;i<count;++i)if(seen_[size_t(first)+i])for(unsigned j=0;j<lanes_;++j){size_t k=(i*lanes_+j)*2;uint16_t n=uint16_t((unsigned(bytes[k])<<8)|bytes[k+1]);if(inputs_[(size_t(first)+i)*lanes_+j]!=n)return false;}
  for(size_t i=0;i<count;++i)if(!seen_[size_t(first)+i]){
   for(unsigned j=0;j<lanes_;++j){size_t k=(i*lanes_+j)*2;inputs_[(size_t(first)+i)*lanes_+j]=uint16_t((unsigned(bytes[k])<<8)|bytes[k+1]);}
   seen_[size_t(first)+i]=1;++received_;
  }
  return true;
 }
 uint64_t Received()const{return received_;}
 uint64_t NextMissing()const{for(size_t i=0;i<seen_.size();++i)if(!seen_[i])return i;return frames_;}
 bool Complete()const{return lanes_&&received_==frames_;}
 std::span<const uint16_t> Inputs()const{return Complete()?std::span<const uint16_t>(inputs_):std::span<const uint16_t>();}
};
}

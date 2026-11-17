// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
#include <map>
#include <span>
#include <vector>
namespace recomp::net {
// Own-process snapshots only. Never populate this cache from received bytes.
class MembershipCheckpoints {
public:
 struct Snapshot {uint32_t fingerprint;std::vector<uint8_t>bytes;};
 static constexpr size_t maxEntries=120,maxBytes=64*1024*1024;
private:
 std::map<uint64_t,Snapshot>states_;size_t bytes_=0;
public:
 void Clear(){states_.clear();bytes_=0;}
 bool Save(int sdkFrame,uint32_t fingerprint,std::span<const uint8_t>ownState){
  if(sdkFrame< -1||ownState.empty()||ownState.size()>maxBytes)return false;
  const uint64_t completed=uint64_t(int64_t(sdkFrame)+1);
  auto erase=states_.lower_bound(completed);
  while(erase!=states_.end()){bytes_-=erase->second.bytes.size();erase=states_.erase(erase);}
  while(!states_.empty()&&(states_.size()>=maxEntries||ownState.size()>maxBytes-bytes_)){auto first=states_.begin();bytes_-=first->second.bytes.size();states_.erase(first);}
  states_.emplace(completed,Snapshot{fingerprint,{ownState.begin(),ownState.end()}});bytes_+=ownState.size();return true;
 }
 const Snapshot* Find(uint64_t completed)const{auto it=states_.find(completed);return it==states_.end()?nullptr:&it->second;}
 size_t Size()const{return states_.size();}size_t Bytes()const{return bytes_;}
};
// The caller supplies the base length from this same live SDK recording.
inline std::span<const uint8_t> ConfirmedRecording(std::span<const uint8_t>recording,size_t base,unsigned lanes){
 if(lanes<2||lanes>4||base<20||recording.size()<base)return {};
 const size_t bytes=recording.size()-base,stride=lanes*2;
 if(bytes%stride||recording.size()<bytes+20)return {};
 const size_t at=recording.size()-16-bytes;
 uint32_t said=0;for(unsigned i=0;i<4;++i)said|=uint32_t(recording[at-4+i])<<(8*i);
 if(said!=bytes)return {};
 for(size_t i=recording.size()-16;i<recording.size();++i)if(recording[i])return {};
 return recording.subspan(at,bytes);
}
}

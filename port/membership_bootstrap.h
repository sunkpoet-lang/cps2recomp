// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "membership_wire.h"
#include "membership_checkpoints.h"
#include <string_view>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#endif
namespace recomp::net {
inline std::optional<ConfirmedInputStaging::Identity> MembershipBootstrap(std::string_view build,std::span<const uint8_t>profile,std::span<const uint8_t>ownInitial){
 if(build.size()!=64||profile.empty()||ownInitial.empty()||ownInitial.size()>MembershipCheckpoints::maxBytes)return {};
 for(char ch:build)if(!((ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')))return {};
#ifdef _WIN32
 BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
 bool ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
 ok=ok&&BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)>=0;
 auto part=[&](std::span<const uint8_t>b){std::vector<uint8_t>length;membership_wire::Put64(length,b.size());if(ok)ok=BCryptHashData(hash,length.data(),ULONG(length.size()),0)>=0;
  while(ok&&!b.empty()){const size_t n=std::min<size_t>(b.size(),1u<<20);ok=BCryptHashData(hash,const_cast<PUCHAR>(b.data()),ULONG(n),0)>=0;b=b.subspan(n);}};
 const std::string_view domain="membership-bootstrap-1";
 part({reinterpret_cast<const uint8_t*>(domain.data()),domain.size()});part({reinterpret_cast<const uint8_t*>(build.data()),build.size()});part(profile);part(ownInitial);
 ConfirmedInputStaging::Identity digest{};if(ok)ok=BCryptFinishHash(hash,digest.data(),ULONG(digest.size()),0)>=0;
 if(hash)BCryptDestroyHash(hash);
 if(alg)BCryptCloseAlgorithmProvider(alg,0);
 if(ok)return digest;
#endif
 return {};
}
inline bool ExportConfirmedPrefix(std::span<const uint8_t>recording,size_t base,unsigned lanes,uint64_t frames,std::vector<uint16_t>&out){
 if(!frames||frames>ConfirmedInputStaging::maxFrames)return false;
 auto input=ConfirmedRecording(recording,base,lanes);const size_t count=size_t(frames)*lanes;
 if(lanes<2||lanes>4||count>input.size()/2)return false;
 std::vector<uint16_t>copy(count);for(size_t i=0;i<count;++i)copy[i]=uint16_t(unsigned(input[2*i])|(unsigned(input[2*i+1])<<8));out=std::move(copy);return true;
}
}

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 the fork's contributors.
#include "rollback_session_owner.h"
#include <stdexcept>
#include <string>
#include <iostream>
#include <type_traits>
#include <cstdint>

struct GekkoSession { GekkoConfig config{}; unsigned actors=0; };
static int creates=0,destroys=0,live=0,delays=0,delayActor=-1,delayValue=-1;
static bool failCreate=false,partialCreate=false,nullSuccess=false,failActor=false;
static std::string trace;
extern "C" bool gekko_create(GekkoSession** out,GekkoSessionType) {
    ++creates;trace+='C';
    if (nullSuccess) return true;
    if (!failCreate || partialCreate) { *out=new GekkoSession; ++live; }
    return !failCreate;
}
extern "C" bool gekko_destroy(GekkoSession** out) {
    if (!out || !*out) return false;
    delete *out;*out=nullptr;++destroys;--live;trace+='D';return true;
}
extern "C" void gekko_start(GekkoSession* s,GekkoConfig* config) {s->config=*config;trace+='S';}
extern "C" void gekko_net_adapter_set(GekkoSession*,GekkoNetAdapter*) {trace+='N';}
extern "C" void gekko_set_disconnect_timeout(GekkoSession*,unsigned) {trace+='T';}
extern "C" int gekko_add_actor(GekkoSession* s,GekkoPlayerType,GekkoNetAddress*) {trace+='A';return failActor?-1:int(s->actors++);}
extern "C" void gekko_set_local_delay(GekkoSession*,int actor,unsigned char delay) {++delays;delayActor=actor;delayValue=delay;trace+='L';}

int main(){using recomp::rollback::Session;unsigned checks=0;auto check=[&](bool ok){if(!ok)throw std::runtime_error("owner check "+std::to_string(checks));++checks;};
 static_assert(!std::is_copy_constructible_v<Session> && !std::is_copy_assignable_v<Session>);
 static_assert(std::is_nothrow_move_constructible_v<Session> && std::is_nothrow_move_assignable_v<Session>);
 GekkoNetAdapter adapter{};uint8_t peer=1;GekkoNetAddress address{&peer,1};
 GekkoConfig config{};config.num_players=2;config.input_size=2;config.state_size=100;config.max_input_prediction_window=8;config.check_distance=10;
 for(unsigned players=2;players<=4;++players)for(auto kind:{GekkoGameSession,GekkoStressSession,GekkoSpectateSession}){
  int oldLive=live,oldDestroy=destroys;trace.clear();
  {Session s;check(!s&&s.Get()==nullptr);config.num_players=static_cast<unsigned char>(players);check(s.Start(kind,config,&adapter,15000));check(trace=="CSNT"&&live==oldLive+1);check(s.Get()->config.num_players==players&&s.Get()->config.check_distance==10&&s.Get()->config.state_size==100);auto raw=s.Get();int oldCreate=creates;check(!s.Start(kind,config));check(s.Get()==raw&&creates==oldCreate);
   for(unsigned p=0;p<players;++p)check(s.AddActor(p==0?GekkoLocalPlayer:GekkoRemotePlayer,p?&address:nullptr)==int(p));
   check(s.Delay(0,8));check(delayActor==0&&delayValue==8);int beforeDelay=delays;check(!s.Delay(-1,0));check(!s.Delay(0,256));check(delays==beforeDelay);
   check(s.AddActor(GekkoRemotePlayer)==-1);GekkoNetAddress empty{};check(s.AddActor(GekkoSpectator,&empty)==-1);failActor=true;check(s.AddActor(GekkoRemotePlayer,&address)==-1);failActor=false;check(s.Get()==raw);
   Session moved(std::move(s));check(!s&&moved.Get()==raw);Session assigned;assigned=std::move(moved);check(!moved&&assigned.Get()==raw);assigned=std::move(assigned);check(assigned.Get()==raw);assigned.Reset();check(!assigned&&live==oldLive&&destroys==oldDestroy+1);assigned.Reset();check(destroys==oldDestroy+1);
  }check(live==oldLive&&destroys==oldDestroy+1);
 }
 config.num_players=2;
 {Session s;check(s.AddActor(GekkoLocalPlayer)==-1);check(!s.Delay(0,0));for(unsigned players:{0u,1u,5u}){auto bad=config;bad.num_players=static_cast<unsigned char>(players);int n=creates;check(!s.Start(GekkoGameSession,bad));check(creates==n);}
  for(unsigned width:{0u,65u}){auto bad=config;bad.input_size=width;check(!s.Start(GekkoGameSession,bad));}auto bad=config;bad.state_size=0;check(!s.Start(GekkoGameSession,bad));check(!s.Start(GekkoReplaySession,config));
  failCreate=true;check(!s.Start(GekkoGameSession,config));check(!s&&live==0);partialCreate=true;check(!s.Start(GekkoGameSession,config));check(!s&&live==0);failCreate=partialCreate=false;nullSuccess=true;check(!s.Start(GekkoGameSession,config));check(!s&&live==0);nullSuccess=false;
 }
 {Session first,second;check(first.Start(GekkoGameSession,config));check(second.Start(GekkoStressSession,config));auto raw=first.Get();int before=destroys;second=std::move(first);check(!first&&second.Get()==raw&&live==1&&destroys==before+1);}
 check(live==0);
 {Session s;for(unsigned i=0;i<5;++i){check(s.Start(GekkoGameSession,config));s.Reset();check(!s&&live==0);}}
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"creates\":"<<creates<<",\"destroys\":"<<destroys<<",\"live\":"<<live<<"}\n";
}

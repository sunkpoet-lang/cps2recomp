// SPDX-License-Identifier: GPL-3.0-only
#include "rollback_transport.h"
#include <recovery.h>
#include <array>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <type_traits>
using namespace recomp::net;
static unsigned checks=0;
static void check(bool ok){++checks;if(!ok)throw std::runtime_error("check "+std::to_string(checks));}
static std::vector<uint8_t> frame(char kind,const std::vector<uint8_t>&payload={}){
 std::vector<uint8_t>b={'N','G','O',uint8_t(kind)};b.insert(b.end(),payload.begin(),payload.end());return b;
}
static void pump(PacketSocket&s){for(int i=0;i<4;++i){s.Pump();std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
static Received receive(Udp&s,std::span<uint8_t>b){Endpoint from{};Received r;for(int i=0;i<100;++i){r=s.Receive(b,from);if(r.kind!=ReceiveKind::WouldBlock)return r;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return r;}
int main(){
 static_assert(std::is_same_v<port::Recovery,Recovery>);
 PacketSocket host;Udp peer,other,watch;std::string error;Endpoint destination{},p{},q{},w{};
 check(host.transport.Open(0,error,BindScope::Loopback));check(peer.Open(0,error,BindScope::Loopback));check(other.Open(0,error,BindScope::Loopback));check(watch.Open(0,error,BindScope::Loopback));
 check(host.transport.Local(destination)&&peer.Local(p)&&other.Local(q)&&watch.Local(w));host.host=true;host.havePeer=true;host.peer=p;
 auto send=[&](Udp&sender,char kind,const std::vector<uint8_t>&data=std::vector<uint8_t>{}){check(sender.Send(frame(kind,data),destination));};
 send(other,'G',{1});send(peer,'Z',{2});check(peer.Send(std::array<uint8_t,3>{'N','G','O'},destination));check(peer.Send(std::array<uint8_t,4>{'B','A','D','G'},destination));pump(host);check(host.forGekko.empty()&&host.forChat.empty());
 send(peer,'G',{0,128,255});send(peer,'C',{7});send(peer,'A',{8});pump(host);check(host.forGekko.size()==1&&host.forGekko.front()==std::vector<uint8_t>({0,128,255}));check(host.forChat.size()==2&&host.forChat[0].first=='C');
 PacketAdapter::active=&host;int count=-1;auto delivered=PacketAdapter::ReceiveData(&count);check(count==1);check(delivered[0]->data_len==3&&std::memcmp(delivered[0]->data,"\0\200\377",3)==0);check(delivered[0]->addr.size==4&&std::memcmp(delivered[0]->addr.data,"peer",4)==0);
 PacketAdapter::FreeData(delivered[0]->addr.data);PacketAdapter::FreeData(delivered[0]->data);PacketAdapter::FreeData(delivered[0]);check(host.forGekko.empty()&&host.forGekkoFrom.empty());host.handedOver.clear();host.forChat.clear();
 host.recovery=true;host.room=0x1020304050607080ULL;host.epoch=2;
 auto tagged=[](uint64_t room,uint64_t epoch){std::vector<uint8_t>b;Recovery::Put(b,room);Recovery::Put(b,epoch);b.push_back(42);return b;};
 send(peer,'G',{1});send(peer,'G',tagged(9,2));send(peer,'C',tagged(host.room,1));send(other,'G',tagged(host.room,2));pump(host);check(host.forGekko.empty()&&host.forChat.empty()&&host.stalePackets==1);
 send(peer,'G',tagged(host.room,2));send(peer,'C',tagged(host.room,2));pump(host);check(host.forGekko.size()==1&&host.forGekko.front()==std::vector<uint8_t>{42});check(host.forChat.size()==1&&host.forChat.front().second==std::vector<uint8_t>{42});host.forGekko.clear();host.forGekkoFrom.clear();host.forChat.clear();
 std::vector<uint8_t>control;Recovery::Put(control,host.room);Recovery::Put(control,3);send(other,'R',control);send(peer,'R',{1});send(peer,'R',control);pump(host);check(host.controls.size()==1&&host.controls[0].first=='R');host.controls.clear();
 host.watchers.emplace_back(w,"spec1");send(watch,'G',tagged(host.room,1));send(watch,'G',tagged(host.room,2));send(watch,'C',tagged(host.room,2));send(watch,'U',{9});pump(host);check(host.stalePackets==2);check(host.forGekko.size()==1&&host.forGekkoFrom.front()=="spec1");check(host.forChat.empty()&&host.fromWatchers.size()==2);check(host.watcherHeard.count("spec1")==1);
 host.forGekko.clear();host.forGekkoFrom.clear();host.fromWatchers.clear();
 // Existing caps are unchanged; pumping between writes avoids testing kernel loss.
 for(int i=0;i<80;++i){send(other,'H',{uint8_t(i)});send(watch,'W',{1});send(peer,'R',control);host.Pump();}pump(host);check(host.hellos.size()==64&&host.watchHellos.size()==64&&host.controls.size()==64);
 for(int i=0;i<280;++i){send(peer,'V',{1});send(watch,'U',{2});host.Pump();}pump(host);check(host.forRelay.size()==256&&host.fromWatchers.size()==256);
 // C callback routes spectators by their name and other traffic to the pinned peer.
 GekkoNetAddress address{};address.data=const_cast<char*>("spec1");address.size=5;PacketAdapter::SendData(&address,"x",1);std::array<uint8_t,128>buffer{};auto got=receive(watch,buffer);check(got.kind==ReceiveKind::Packet&&got.size==21&&buffer[3]=='G'&&buffer[20]=='x');
 PacketAdapter::SendData(nullptr,"y",1);got=receive(peer,buffer);check(got.kind==ReceiveKind::Packet&&got.size==21&&buffer[20]=='y');check(host.sentToPeer==21&&host.sentToWatchers==21&&host.packetsToWatchers==1);
 host.Refuse("full",q);got=receive(other,buffer);check(got.kind==ReceiveKind::Packet&&got.size==8&&buffer[3]=='X');host.Refuse("full",q);Endpoint source{};check(other.Receive(buffer,source).kind==ReceiveKind::WouldBlock);check(host.refusedAt.size()==1);
 PacketSocket guest;check(guest.transport.Open(0,error,BindScope::Loopback));check(guest.transport.Local(destination));guest.peer=p;guest.havePeer=true;
 send(other,'X',{'n','o'});send(peer,'X',{'f','u','l','l'});send(other,'Q',std::vector<uint8_t>(20,1));send(peer,'Q',std::vector<uint8_t>(20,2));send(peer,'W',{1});send(peer,'V',{3});pump(guest);check(guest.refused=="full"&&guest.status==std::vector<uint8_t>(20,2)&&guest.watchHellos.empty()&&guest.forRelay.size()==1);
 PacketAdapter::active=nullptr;PacketAdapter::SendData(nullptr,"ignored",7);
 // Three independent host/guest barriers, not a multi-party membership claim.
 for(int peerIndex=0;peerIndex<3;++peerIndex){Recovery h,g;h.Reset(true,0);g.Reset(false,0);uint64_t now=10;
  for(uint64_t epoch=2;epoch<=5;++epoch){auto r=h.Begin(now);check(r.reply=='R'&&r.epoch==epoch&&!r.reset);check(!h.Begin(now+1).accepted);auto deadline=h.Deadline();
   r=h.Tick(now+251);check(r.reply=='R');check(!g.Receive('R',epoch+1,now+252).accepted);auto k=g.Receive('R',epoch,now+253);check(k.reply=='K'&&!k.reset);check(g.Receive('R',epoch,now+254).reply=='K');
   auto s=h.Receive('K',epoch,now+255);check(s.reset&&s.reply=='S');check(!h.Receive('K',epoch,now+256).reset);auto t=g.Receive('S',epoch,now+257);check(t.reset&&t.ready&&t.reply=='T');check(!g.Receive('S',epoch,now+258).reset);check(h.Receive('T',epoch,now+259).ready);
   check(!h.Waiting()&&!g.Waiting()&&h.Epoch()==epoch&&g.Epoch()==epoch);check(h.Deadline()==deadline);h.Complete();g.Complete();check(!h.Deadline()&&!g.Deadline());check(!h.Receive('N',epoch-1,now+260).accepted);now+=1000;
  }
  h.Begin(now);auto deadline=h.Deadline();h.Receive('N',h.Epoch(),now+100);h.Receive('P',h.Epoch(),now+101);check(h.Deadline()==deadline);check(!h.Tick(deadline-1).expired&&h.Tick(deadline).expired);check(!h.Receive('K',h.Epoch()+1,deadline+1).reset);
  g.Begin(now);check(g.Receive('L',g.Epoch(),now+1).left&&!g.Waiting());
 }
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"network\":\"loopback-only\",\"independent_recovery_pairs\":3}\n";
}

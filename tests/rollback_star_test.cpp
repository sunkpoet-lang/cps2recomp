// SPDX-License-Identifier: GPL-3.0-only
#include "rollback_star.h"
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace recomp::net;
static unsigned checks=0;static uint64_t clock_=0;
static void check(bool b){++checks;if(!b)throw std::runtime_error("check "+std::to_string(checks));}
static void step(std::initializer_list<StarRoom*>rooms,unsigned n=100){for(unsigned i=0;i<n;++i){clock_+=10;for(auto*r:rooms){r->Pump(clock_);r->Tick(clock_);}std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
static std::vector<uint8_t> wire(char kind,unsigned source,unsigned target,uint64_t room,uint64_t token,const std::vector<uint8_t>&body){std::vector<uint8_t>b={'R','S','T','1',1,uint8_t(kind),uint8_t(source),uint8_t(target)};for(uint64_t v:{room,token})for(int i=7;i>=0;--i)b.push_back(uint8_t(v>>(8*i)));b.insert(b.end(),body.begin(),body.end());return b;}
int main(){
 std::string error;std::vector<uint8_t>profile={'a','b','c'};StarRoom host,a,b;Endpoint at{};
 check(host.Open(true,0,"",101,3,1,"Host",profile,error,BindScope::Loopback));check(host.Local(at));auto port=ntohs(at.sin_port);
 check(a.Open(false,port,"127.0.0.1",201,3,1,"Alice",profile,error,BindScope::Loopback));step({&host,&a},40);check(a.Primary()==1&&a.Mask()==2&&host.Filled()==3&&!host.Ready());
 step({&host,&a},80);check(a.Primary()==1&&a.Mask()==2&&host.Filled()==3); // duplicate/retried hello/offer does not reallocate
 check(b.Open(false,port,"127.0.0.1",301,3,1,"Bob",profile,error,BindScope::Loopback));step({&host,&a,&b});check(host.Ready()&&a.Ready()&&b.Ready());check(b.Primary()==2&&b.Mask()==4&&host.Filled()==7&&a.Filled()==7);check(a.Names()[2]=="Bob"&&b.Names()[1]=="Alice");
 check(a.Send(2,std::array<uint8_t,3>{0,128,255}));step({&host,&a,&b},10);check(host.relayed==1&&b.incoming.size()==1);check(b.incoming.front().source==1&&b.incoming.front().bytes==std::vector<uint8_t>({0,128,255}));b.incoming.clear();
 check(b.Send(0,std::array<uint8_t,1>{42}));check(host.Send(1,std::array<uint8_t,1>{43}));step({&host,&a,&b},10);check(host.incoming.size()==1&&host.incoming.front().source==2&&host.incoming.front().bytes[0]==42);check(a.incoming.size()==1&&a.incoming.front().source==0&&a.incoming.front().bytes[0]==43);host.incoming.clear();a.incoming.clear();
 for(auto*r:{&a,&b}){check(r->sentTo[0]>0&&r->receivedFrom[0]>0);for(unsigned i=1;i<4;++i)check(r->sentTo[i]==0&&r->receivedFrom[i]==0);}
 check(!a.Send(1,std::array<uint8_t,1>{1}));check(!a.Send(3,std::array<uint8_t,1>{1}));check(!a.Send(0,{}));std::vector<uint8_t>large(4097);check(!a.Send(0,large));
 StarRoom fourth;check(fourth.Open(false,port,"127.0.0.1",401,3,1,"Fourth",profile,error,BindScope::Loopback));step({&host,&a,&b,&fourth},50);check(fourth.Refusal()=="The match is full"&&host.Filled()==7&&host.Ready());
 StarRoom mismatch;check(mismatch.Open(false,port,"127.0.0.1",501,3,1,"Other",std::array<uint8_t,1>{9},error,BindScope::Loopback));step({&host,&mismatch},50);check(mismatch.Refusal().find("do not match")!=std::string::npos&&host.Filled()==7);
 Udp stranger;check(stranger.Open(0,error,BindScope::Loopback));auto rejects=host.rejected;check(stranger.Send(wire('G',1,0,101,201,{1}),at));step({&host},5);check(host.rejected>rejects&&host.incoming.empty());
 // Adapter routes logical source names, and returned buffers have C allocator ownership.
 StarAdapter::active=&a;auto addr=StarAdapter::Address(2);StarAdapter::SendData(&addr,"xyz",3);step({&host,&a,&b},10);StarAdapter::active=&b;int n=-1;auto results=StarAdapter::ReceiveData(&n);check(n==1&&results[0]->data_len==3&&std::memcmp(results[0]->data,"xyz",3)==0);check(results[0]->addr.size==5&&std::memcmp(results[0]->addr.data,"seat1",5)==0);
 StarAdapter::FreeData(results[0]->addr.data);StarAdapter::FreeData(results[0]->data);StarAdapter::FreeData(results[0]);StarAdapter::handed.clear();StarAdapter::active=nullptr;
 // A two-seat admission is atomic. Actual two-local Gekko play belongs to 2.2.
 StarRoom h4,g2,g1;check(h4.Open(true,0,"",601,4,1,"H4",profile,error,BindScope::Loopback));check(h4.Local(at));port=ntohs(at.sin_port);
 check(g2.Open(false,port,"127.0.0.1",602,4,2,"Double",profile,error,BindScope::Loopback));step({&h4,&g2},40);check(g2.Mask()==6&&g2.Primary()==1&&h4.Filled()==7&&!h4.Ready());
 check(g1.Open(false,port,"127.0.0.1",603,4,1,"Last",profile,error,BindScope::Loopback));step({&h4,&g2,&g1});check(h4.Ready()&&g2.Ready()&&g1.Ready()&&g1.Mask()==8&&g2.Names()[2]=="Double 2");
 StarRoom h3,one,two;check(h3.Open(true,0,"",701,3,1,"H3",profile,error,BindScope::Loopback));check(h3.Local(at));port=ntohs(at.sin_port);check(one.Open(false,port,"127.0.0.1",702,3,1,"One",profile,error,BindScope::Loopback));step({&h3,&one},40);
 check(two.Open(false,port,"127.0.0.1",703,3,2,"Two",profile,error,BindScope::Loopback));step({&h3,&one,&two},50);check(two.Refusal()=="The match is full"&&h3.Filled()==3);
 // An admitted physical peer cannot impersonate another source, room or token.
 StarRoom rawHost;check(rawHost.Open(true,0,"",801,2,1,"Raw",profile,error,BindScope::Loopback));check(rawHost.Local(at));Udp raw;check(raw.Open(0,error,BindScope::Loopback));check(raw.Send(wire('H',255,0,0,802,{1,1,0,3,'P','a','b','c'}),at));step({&rawHost},5);check(rawHost.Filled()==3);
 rejects=rawHost.rejected;
 for(auto packet:{wire('G',0,0,801,802,{1}),wire('G',1,0,999,802,{1}),wire('G',1,0,801,999,{1}),wire('G',1,3,801,802,{1}),wire('G',1,0,801,802,{})})check(raw.Send(packet,at));
 step({&rawHost},5);check(rawHost.rejected>=rejects+5&&rawHost.incoming.empty());check(raw.Send(wire('G',1,0,801,802,{5}),at));step({&rawHost},5);check(rawHost.incoming.size()==1&&rawHost.incoming.front().source==1&&rawHost.incoming.front().bytes[0]==5);rawHost.incoming.clear();
 for(unsigned i=0;i<530;++i){check(raw.Send(wire('G',1,0,801,802,{6}),at));if(i%20==0)rawHost.Pump(clock_);}
 step({&rawHost},5);check(rawHost.incoming.size()==512);
 StarRoom invalid;check(!invalid.Open(true,0,"",0,3,1,"Bad",profile,error));check(!invalid.Open(true,0,"",1,5,1,"Bad",profile,error));check(!invalid.Open(false,1,"127.0.0.1",1,3,1,"Bad\n",profile,error));
 Udp fakeHost;check(fakeHost.Open(0,error,BindScope::Loopback));check(fakeHost.Local(at));StarRoom victim;check(victim.Open(false,ntohs(at.sin_port),"127.0.0.1",901,3,1,"Victim",profile,error,BindScope::Loopback));check(victim.Local(at));
 check(fakeHost.Send(wire('X',0,255,900,901,{99}),at));step({&victim},5);check(victim.Refusal().empty()&&victim.rejected>0);
 check(fakeHost.Send(wire('X',0,255,900,902,{1}),at));step({&victim},5);check(victim.Refusal().empty());
 check(fakeHost.Send(wire('X',0,255,900,901,{1}),at));step({&victim},5);check(victim.Refusal()=="The match is full");
 StarRoom h22,g22;check(h22.Open(true,0,"",1001,4,2,"Host22",profile,error,BindScope::Loopback));check(h22.Local(at));check(h22.Mask()==3&&h22.Filled()==3&&h22.Owns(0)&&h22.Owns(1)&&!h22.Owns(2)&&h22.Owner(1)==0);
 check(g22.Open(false,ntohs(at.sin_port),"127.0.0.1",1002,4,2,"Guest22",profile,error,BindScope::Loopback));step({&h22,&g22});check(h22.Ready()&&g22.Ready()&&g22.Mask()==12);
 check(g22.Owner(0)==0&&g22.Owner(1)==0&&g22.Owner(2)==2&&g22.Owner(3)==2&&g22.Owner(4)==255&&!g22.Owns(4));
 check(g22.Names()[1]=="Host22 2"&&h22.Names()[3]=="Guest22 2");
 check(h22.Send(3,std::array<uint8_t,1>{65}));check(g22.Send(1,std::array<uint8_t,1>{66}));step({&h22,&g22},10);
 check(g22.incoming.size()==1&&g22.incoming.front().source==0&&g22.incoming.front().bytes[0]==65);
 check(h22.incoming.size()==1&&h22.incoming.front().source==2&&h22.incoming.front().bytes[0]==66);
 check(!h22.Send(1,std::array<uint8_t,1>{1})&&!g22.Send(3,std::array<uint8_t,1>{1}));check(StarAdapter::Address(4).data==nullptr);
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"network\":\"loopback-only\",\"three_seats\":true,\"two_seat_admission\":true,\"host_two_seats\":true}\n";
}

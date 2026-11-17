#include "rollback_star.h"
#include <thread>
#include <cstdio>
#include <cstdlib>
using namespace recomp::net;
static unsigned checks=0;static uint64_t now=0;
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static void step(std::initializer_list<StarRoom*>rooms,unsigned n=80){for(unsigned i=0;i<n;++i){now+=5;for(auto*r:rooms){r->Pump(now);r->Tick(now);}std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
static std::vector<uint8_t>wire(char kind,uint64_t room,uint64_t token,std::span<const uint8_t>body){std::vector<uint8_t>b={'R','S','T','1',1,uint8_t(kind),1,0};membership_wire::Put64(b,room);membership_wire::Put64(b,token);b.insert(b.end(),body.begin(),body.end());return b;}
int main(){
 StarRoom host,a,b;Endpoint at{};std::string error;std::array<uint8_t,1>profile{7};
 C(host.Open(true,0,"",100,3,1,"H",profile,error,BindScope::Loopback));C(host.Local(at));auto port=ntohs(at.sin_port);
 C(a.Open(false,port,"127.0.0.1",200,3,1,"A",profile,error,BindScope::Loopback));step({&host,&a});C(a.Primary()==1);
 C(b.Open(false,port,"127.0.0.1",300,3,1,"B",profile,error,BindScope::Loopback));step({&host,&a,&b},200);
 for(auto*r:{&host,&a,&b}){C(r->AttachMembership());C(r->EnableEpochFraming());C(r->EpochFramed());}
 std::vector<uint8_t>payload(4096,17);C(a.Send(2,payload));step({&host,&a,&b});C(b.incoming.size()==1&&b.incoming.front().bytes==payload);C(b.incoming.front().epoch==1);C(b.ValidQueued(b.incoming.front()));b.incoming.clear();
 C(host.Send(1,std::array<uint8_t,2>{8,9}));step({&host,&a,&b});C(a.incoming.front().bytes==std::vector<uint8_t>({8,9}));a.incoming.clear();payload.push_back(1);C(!a.Send(2,payload));
 StarRoom rawHost;Udp raw;C(rawHost.Open(true,0,"",400,2,1,"Raw",profile,error,BindScope::Loopback));C(rawHost.Local(at));C(raw.Open(0,error,BindScope::Loopback));
 auto hello=wire('H',0,500,std::array<uint8_t,6>{1,1,0,1,'R',7});hello[6]=255;C(raw.Send(hello,at));step({&rawHost});C(raw.Send(wire('A',400,500,std::array<uint8_t,1>{3}),at));step({&rawHost});C(rawHost.Ready());C(rawHost.AttachMembership());C(rawHost.EnableEpochFraming());
 auto rejected=rawHost.rejected;auto data=std::array<uint8_t,2>{3,4};
 C(raw.Send(wire('G',400,500,data),at));C(raw.Send(wire('G',400,500,membership_wire::Game(2,data)),at));step({&rawHost});C(rawHost.rejected>=rejected+2&&rawHost.incoming.empty());
 C(raw.Send(wire('G',400,500,membership_wire::Game(1,data)),at));step({&rawHost});C(rawHost.incoming.size()==1&&rawHost.incoming.front().bytes==std::vector<uint8_t>({3,4}));
 auto*channel=rawHost.Membership();auto prepare=channel->Begin(1,1500,123,now);C(prepare.send.has_value());C(channel->Stage(MembershipRoster{2,1,{0,255,255,255}}));C(channel->Verify(1500,123,now).apply);C(!rawHost.ValidQueued(rawHost.incoming.front()));C(channel->Publish(true).accepted);C(!rawHost.ValidQueued(rawHost.incoming.front()));
 StarAdapter::active=&rawHost;int n=-1;StarAdapter::ReceiveData(&n);C(n==0&&rawHost.incoming.empty());StarAdapter::active=nullptr;
 C(raw.Send(wire('G',400,500,membership_wire::Game(1,data)),at));step({&rawHost});C(rawHost.incoming.empty());
 std::printf("%u epoch game framing checks passed; host relay, stale/queued traffic and 4096-byte payload\n",checks);
}

#include "rollback_star.h"
#include <thread>
#include <cstdio>
#include <cstdlib>
using namespace recomp::net;
static unsigned n=0;static uint64_t now=0;
#define C(x) do{++n;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static void step(std::initializer_list<StarRoom*>rooms,unsigned count=160){for(unsigned i=0;i<count;++i){now+=5;for(auto*r:rooms){r->Pump(now);r->Tick(now);}std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
int main(){StarRoom host,a,late;Endpoint at{};std::string error;std::array<uint8_t,1>profile{2};
 C(host.Open(true,0,"",10,3,1,"H",profile,error,BindScope::Loopback,2));C(!host.Ready());C(host.Local(at));auto port=ntohs(at.sin_port);
 C(a.Open(false,port,"127.0.0.1",20,3,1,"A",profile,error,BindScope::Loopback,2));step({&host,&a});C(host.Ready()&&a.Ready());C(host.Filled()==3&&a.Filled()==3);C(host.Mask()==1&&a.Mask()==2);C(host.Owner(2)==255&&a.Owner(2)==255);C(host.RoutingOwner(2)==0&&a.RoutingOwner(2)==0);C(host.SdkLocal(2)&&!host.Owns(2)&&!a.SdkLocal(2));C(host.RoutingOwner(4)==255);C(host.Names()[2].empty());
 C(host.AttachMembership()&&a.AttachMembership());C(host.EnableEpochFraming()&&a.EnableEpochFraming());C(host.Membership()->Active().members==3);C(host.Membership()->Active().owners[2]==255);
 C(late.Open(false,port,"127.0.0.1",30,3,1,"Late",profile,error,BindScope::Loopback,2));step({&host,&a,&late});C(late.Refusal()=="The match is full");C(host.Filled()==3&&host.Owner(2)==255);
 C(a.Send(0,std::array<uint8_t,1>{7}));step({&host,&a});C(host.incoming.size()==1&&host.incoming.front().bytes[0]==7);
 StarRoom invalid;C(!invalid.Open(true,0,"",40,3,1,"Bad",profile,error,BindScope::Loopback,4));C(!invalid.Open(true,0,"",40,3,2,"Bad",profile,error,BindScope::Loopback,2));
 std::printf("%u partial initial room checks passed; neutral host-owned SDK slot retains no physical seat\n",n);
}

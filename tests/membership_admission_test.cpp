#include "rollback_star.h"
#include <cstdio>
#include <cstdlib>
#include <thread>
using namespace recomp::net;
static unsigned checks=0;static uint64_t now=0;
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static void step(std::initializer_list<StarRoom*>rooms,unsigned n=40){for(unsigned i=0;i<n;++i){now+=5;for(auto*r:rooms){r->Tick(now);r->Pump(now);}std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
int main(){
 StarRoom host,a,b;Endpoint endpoint{};std::string error;std::vector<uint8_t>profile{'b','o','o','t','s','t','r','a','p','-','1','|',42};
 C(host.Open(true,0,"",100,3,1,"Host",profile,error,BindScope::Loopback));C(host.Local(endpoint));auto port=ntohs(endpoint.sin_port);C(!host.AttachMembership());
 StarRoom mismatch;auto other=profile;other.back()^=1;C(mismatch.Open(false,port,"127.0.0.1",999,3,1,"WrongBootstrap",other,error,BindScope::Loopback));step({&host,&mismatch});C(mismatch.Refusal().find("do not match")!=std::string::npos);C(host.Filled()==1);
 C(a.Open(false,port,"127.0.0.1",200,3,1,"A",profile,error,BindScope::Loopback));step({&host,&a},80);C(a.Primary()==1);C(b.Open(false,port,"127.0.0.1",300,3,1,"B",profile,error,BindScope::Loopback));step({&host,&a,&b},200);C(host.Ready()&&a.Ready()&&b.Ready());
 C(host.AttachMembership()&&a.AttachMembership()&&b.AttachMembership());C(!host.AttachMembership());
 C(a.Send(2,std::array<uint8_t,1>{77}));step({&host,&a,&b});C(b.incoming.size()==1);b.incoming.clear();
 MembershipRoster next{3,3,{0,1,255,255}};auto proposal=host.Membership()->Begin(3,1500,123,now);C(proposal.send.has_value());C(host.Membership()->Stage(next));C(host.SendMembership(1,proposal)&&host.SendMembership(2,proposal));step({&host,&a,&b});
 auto ae=a.TakeMembership(),be=b.TakeMembership();C(ae&&ae->accepted);C(be&&be->departed);C(a.Membership()->Stage(next));C(a.incoming.empty()&&b.incoming.empty());
 C(host.Membership()->Verify(1500,123,now).accepted);auto ready=a.Membership()->Verify(1500,123,now);C(a.SendMembership(0,ready));step({&host,&a,&b});auto effect=host.TakeMembership();C(effect&&effect->apply&&!effect->send);C(host.Membership()->Active().members==7);
 auto commit=host.Membership()->Publish(true);C(host.SendMembership(1,commit));step({&host,&a,&b});effect=a.TakeMembership();C(effect&&effect->apply&&!effect->send);C(a.SendMembership(0,a.Membership()->Publish(true)));step({&host,&a,&b});effect=host.TakeMembership();C(effect&&effect->send&&effect->send->kind==MembershipEpoch::Kind::Start);C(host.SendMembership(1,*effect));step({&host,&a,&b});effect=a.TakeMembership();C(effect&&effect->send);C(a.SendMembership(0,*effect));step({&host,&a,&b});C(host.TakeMembership().has_value());C(host.Membership()->MayAdvance()&&a.Membership()->MayAdvance());
 // Channel publication does not silently rewrite the room or its SDK adapter.
 C(host.Owner(2)==2&&host.Filled()==7);C(host.incoming.empty()&&a.incoming.empty());
 Udp outsider;C(outsider.Open(0,error,BindScope::Loopback));auto forged=membership_wire::Encode(1,0,100,200,*ready.send);auto rejected=host.rejected;C(outsider.Send(forged,endpoint));step({&host});C(host.rejected>rejected);C(!host.TakeMembership());
 auto again=host.Membership()->Begin(7,2000,456,now);C(again.send.has_value());
 for(unsigned i=0;i<80;++i){C(host.SendMembership(1,again));}step({&host,&a});C(a.Membership()->Failed());
 effect=a.TakeMembership();C(effect&&effect->failed);C(!a.TakeMembership());C(a.Membership()->Active().members==3);
 std::printf("%u shared StarRoom control/identity checks passed (3 UDP rooms)\n",checks);
}

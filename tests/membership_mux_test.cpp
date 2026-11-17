#include "rollback_star.h"
#include <cstdio>
#include <cstdlib>
#include <thread>
using namespace recomp::net;
static unsigned checks=0;static uint64_t now=0;
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static void step(std::initializer_list<StarRoom*>rooms,unsigned n=40){for(unsigned i=0;i<n;++i){now+=5;for(auto*r:rooms){r->Tick(now);r->Pump(now);}std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
int main(){
 StarRoom host,a,b;Endpoint at{};std::string error;std::array<uint8_t,2>profile{1,2};
 C(host.Open(true,0,"",100,3,1,"Host",profile,error,BindScope::Loopback));C(host.Local(at));auto port=ntohs(at.sin_port);
 C(a.Open(false,port,"127.0.0.1",200,3,1,"A",profile,error,BindScope::Loopback));step({&host,&a},80);C(a.Primary()==1);C(b.Open(false,port,"127.0.0.1",300,3,1,"B",profile,error,BindScope::Loopback));step({&host,&a,&b},200);
 C(host.AttachMembership()&&a.AttachMembership()&&b.AttachMembership());MembershipRoster next{3,3,{0,1,255,255}};
 auto proposal=host.Membership()->Begin(3,1500,123,now);C(host.Membership()->Stage(next));C(host.SendMembership(1,proposal));step({&host,&a,&b});C(a.TakeMembership().has_value());C(a.Membership()->Stage(next));
 ConfirmedInputStaging::Identity id{};id[0]=9;std::vector<uint16_t>data(4500);for(size_t i=0;i<data.size();++i)data[i]=uint16_t(i*19);
 C(host.StartJournal(1,id,3,now,data));C(a.StartJournal(0,id,3,now));C(!host.StartJournal(1,id,3,now,data));C(!a.StartJournal(2,id,3,now));
 C(b.Send(1,std::array<uint8_t,2>{55,66}));
 for(unsigned i=0;i<1000&&(!a.Journal(0)||!a.Journal(0)->Complete());++i)step({&host,&a,&b},1);
 C(a.Journal(0)&&a.Journal(0)->Complete());C(std::equal(data.begin(),data.end(),a.Journal(0)->Inputs().begin()));C(a.incoming.size()==1&&a.incoming.front().bytes==std::vector<uint8_t>({55,66}));C(host.incoming.empty()&&b.incoming.empty());C(a.Membership()->Epoch().State()==MembershipEpoch::Stage::Preparing);
 auto abort=host.Membership()->Abort();C(host.SendMembership(1,abort));step({&host,&a,&b});C(!host.Journal(1)&&!a.Journal(0));C(a.TakeMembership()->resumeOld);
 // New transaction has fresh timers and cannot reuse the previous completed transfer.
 proposal=host.Membership()->Begin(3,1500,123,now);C(host.Membership()->Stage(next));C(host.SendMembership(1,proposal));step({&host,&a,&b});C(a.TakeMembership().has_value());C(a.Membership()->Stage(next));
 C(a.StartJournal(0,id,3,now));now+=5000;a.Tick(now);C(a.Journal(0)&&a.Journal(0)->Failed());C(a.Journal(0)->Inputs().empty());a.CancelJournals();C(!a.Journal(0));
 std::printf("%u shared-socket journal checks passed (3 StarRooms, concurrent game traffic)\n",checks);
}


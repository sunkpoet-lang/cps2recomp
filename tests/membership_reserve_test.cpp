#include "rollback_star.h"
#include <thread>
#include <cstdio>
#include <cstdlib>
using namespace recomp::net;
static unsigned n=0;static uint64_t now=0;
#define C(x) do{++n;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static void step(std::initializer_list<StarRoom*>rooms,unsigned count=100){for(unsigned i=0;i<count;++i){now+=5;for(auto*r:rooms){r->Pump(now);r->Tick(now);}std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
static std::vector<uint8_t>ticket(uint64_t id,uint64_t token=701){std::vector<uint8_t>b={'R','S','T','1',1,'P',0,255};membership_wire::Put64(b,600);membership_wire::Put64(b,token);b.push_back(3);b.push_back(4);membership_wire::Put64(b,id);membership_wire::Put64(b,1);b.push_back(0);b.push_back(1);b.push_back(7);return b;}
int main(){StarRoom host,a,b,other;Endpoint at{};std::string error;std::array<uint8_t,1>profile{7};
 C(host.Open(true,0,"",100,3,1,"H",profile,error,BindScope::Loopback,2));C(host.Local(at));auto port=ntohs(at.sin_port);C(!host.EnableLateReservations());
 C(a.Open(false,port,"127.0.0.1",200,3,1,"A",profile,error,BindScope::Loopback,2));step({&host,&a},200);C(host.Ready()&&a.Ready());C(host.AttachMembership()&&a.AttachMembership());C(host.EnableEpochFraming()&&a.EnableEpochFraming());C(host.EnableLateReservations());
 C(b.Open(false,port,"127.0.0.1",300,3,1,"B",profile,error,BindScope::Loopback,2));step({&host,&a,&b});auto id=host.ReservationId(),deadline=host.ReservationDeadline(),clientDeadline=b.ProvisionalDeadline();C(id&&b.ProvisionalId()==id&&b.ProvisionalMask()==4);C(!b.Ready()&&b.Primary()==255&&b.Mask()==0&&b.Filled()==0);C(host.Filled()==3&&host.Owner(2)==255&&host.Names()[2].empty());C(host.Membership()->Active().members==3);C(a.Send(0,std::array<uint8_t,1>{9}));step({&host,&a,&b});C(host.incoming.size()==1&&host.incoming.front().bytes[0]==9);host.incoming.clear();C(host.ReservationId()==id&&host.ReservationDeadline()==deadline&&b.ProvisionalDeadline()==clientDeadline);
 C(other.Open(false,port,"127.0.0.1",400,3,1,"Other",profile,error,BindScope::Loopback,2));step({&host,&a,&b,&other});C(other.Refusal()=="The match is full");C(host.ReservationId()==id);
 host.CancelReservation();step({&host,&a,&b});C(host.ReservationId()>id&&b.ProvisionalId()==host.ReservationId());C(host.Filled()==3&&host.Owner(2)==255);now+=30000;host.Tick(now);b.Tick(now);C(!host.ReservationId()&&!b.ProvisionalId());
 Udp fake,stranger;C(fake.Open(0,error,BindScope::Loopback));C(fake.Local(at));StarRoom victim;C(victim.Open(false,ntohs(at.sin_port),"127.0.0.1",701,3,1,"V",profile,error,BindScope::Loopback,2));C(victim.Local(at));
 C(fake.Send(ticket(1),at));step({&victim});C(victim.ProvisionalId()==1&&!victim.Ready()&&victim.Mask()==0);auto until=victim.ProvisionalDeadline();C(fake.Send(ticket(1),at));step({&victim});C(victim.ProvisionalDeadline()==until);
 C(stranger.Open(0,error,BindScope::Loopback));C(stranger.Send(ticket(9),at));auto bad=ticket(2);bad.back()^=1;C(fake.Send(bad,at));C(fake.Send(ticket(2,999),at));step({&victim});C(victim.ProvisionalId()==1);
 for(size_t length=0;length<ticket(2).size();++length){bad=ticket(2);bad.resize(length);C(fake.Send(bad,at));}
 for(size_t pos:{size_t(4),size_t(6),size_t(7),size_t(8),size_t(16),size_t(24),size_t(25),size_t(42)}){bad=ticket(2);bad[pos]^=128;C(fake.Send(bad,at));}
 bad=ticket(0);C(fake.Send(bad,at));bad=ticket(2);for(unsigned j=34;j<42;++j)bad[j]=0;C(fake.Send(bad,at));bad=ticket(2);bad.push_back(0);C(fake.Send(bad,at));step({&victim});C(victim.ProvisionalId()==1&&victim.ProvisionalDeadline()==until);
 C(fake.Send(ticket(3),at));step({&victim});C(victim.ProvisionalId()==3);C(fake.Send(ticket(2),at));step({&victim});C(victim.ProvisionalId()==3);C(victim.Primary()==255&&victim.Filled()==0);
 std::printf("%u late reservation checks passed; provisional ticket never activates a seat\n",n);
}

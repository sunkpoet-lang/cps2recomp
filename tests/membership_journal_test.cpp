#include "membership_journal.h"
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdlib>
using namespace recomp::net;
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
 Udp hs,gs,outsider;Endpoint h{},g{},o{},from{};std::string error;
 C(hs.Open(0,error,BindScope::Loopback));C(gs.Open(0,error,BindScope::Loopback));C(outsider.Open(0,error,BindScope::Loopback));C(hs.Local(h)&&gs.Local(g)&&outsider.Local(o));
 MembershipJournal::Contract contract{0,2,3,11,22,33,4,1500,{}};contract.identity[31]=77;
 std::vector<uint16_t>confirmed(4500);for(size_t i=0;i<confirmed.size();++i)confirmed[i]=uint16_t(i*17+3);
 MembershipJournal host,guest;C(host.Open(true,g,contract,0,confirmed));C(guest.Open(false,h,contract,0));
 auto recv=[&](Udp&socket){std::array<uint8_t,2048>b{};auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);for(;;){auto r=socket.Receive(b,from);if(r.kind==ReceiveKind::Packet)return std::vector<uint8_t>(b.begin(),b.begin()+r.size);C(r.kind==ReceiveKind::WouldBlock);C(std::chrono::steady_clock::now()<until);std::this_thread::sleep_for(std::chrono::milliseconds(1));}};
 auto request=guest.Request(0);C(request.size()==89);C(guest.Request(99).empty()); // first request deliberately lost
 request=guest.Request(100);C(!request.empty());auto forged=request;forged[24]^=1;C(host.Receive(g,forged,100).empty());C(host.Receive(o,request,100).empty());
 for(size_t n=0;n<89;++n)C(host.Receive(g,std::span(request).first(n),100).empty());
 for(size_t pos:{size_t(0),size_t(4),size_t(5),size_t(6),size_t(7),size_t(8),size_t(16),size_t(24),size_t(32),size_t(40),size_t(48),size_t(79),size_t(80),size_t(81),size_t(88)}){forged=request;forged[pos]^=128;C(host.Receive(g,forged,100).empty());}
 forged=request;forged.push_back(0);C(host.Receive(g,forged,100).empty());
 C(gs.Send(request,h));auto packet=recv(hs);auto response=host.Receive(from,packet,100);C(response.size()==809); // first response deliberately lost
 C(host.Receive(g,request,101).empty()); // response throttle
 request=guest.Request(200);C(gs.Send(request,h));packet=recv(hs);response=host.Receive(from,packet,200);C(hs.Send(response,g));packet=recv(gs);guest.Receive(from,packet,201);C(!guest.Complete());
 for(size_t n=0;n<packet.size();++n){guest.Receive(h,std::span(packet).first(n),202);C(!guest.Failed());}
 auto stale=packet;stale[32]^=1;guest.Receive(h,stale,202);C(!guest.Failed());
 unsigned blocks=1;for(uint64_t now=300;now<3000&&!guest.Complete();now+=100){request=guest.Request(now);C(request.size()==89);C(gs.Send(request,h));packet=recv(hs);response=host.Receive(from,packet,now);C(!response.empty());C(hs.Send(response,g));packet=recv(gs);guest.Receive(from,packet,now+1);++blocks;}
 C(blocks==13);C(guest.Complete());C(std::equal(confirmed.begin(),confirmed.end(),guest.Inputs().begin()));
 guest.Receive(h,stale,9000);C(guest.Complete());C(guest.Request(9000).empty());
 MembershipJournal timeout;C(timeout.Open(false,h,contract,0));C(!timeout.Request(4999).empty());C(timeout.Request(5000).empty());C(timeout.Failed());C(timeout.Inputs().empty());
 // Pre-completion conflicting duplicates are fatal, with no exposed partial journal.
 MembershipJournal other,source;C(other.Open(false,h,contract,0));C(source.Open(true,g,contract,0,confirmed));request=other.Request(0);response=source.Receive(g,request,0);other.Receive(h,response,1);response.back()^=1;other.Receive(h,response,2);C(other.Failed());C(other.Inputs().empty());
 auto invalid=contract;invalid.frames=UINT64_MAX;MembershipJournal bad;C(!bad.Open(false,h,invalid,0));C(!bad.Open(true,g,contract,0,std::span(confirmed).first(3)));
 std::printf("%u journal transfer checks passed; 1500 frames/3 lanes, actual Windows UDP\n",checks);
}

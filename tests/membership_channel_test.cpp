#include "membership_channel.h"
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdlib>
using namespace recomp::net;
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
 MembershipRoster old{3,3,{0,1,255,255}},next{3,7,{0,1,2,255}};
 C(old.Valid());C(next.Extends(old));C(old.RoutingOwner(2)==0);C(old.RoutingOwner(4)==255);
 auto invalid=next;invalid.owners={1,0,2,255};C(!invalid.Valid());invalid=next;invalid.owners={0,2,1,255};C(!invalid.Extends(old));
 std::array<Udp,3>sockets;std::array<Endpoint,3>at{};std::array<MembershipChannel,3>c;std::string error;
 for(unsigned i=0;i<3;++i){C(sockets[i].Open(0,error,BindScope::Loopback));C(sockets[i].Local(at[i]));C(c[i].Reset(i,999,1,old));}
 for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)if(i!=j)C(c[i].PinPeer(j,at[j],100+std::max(i,j)*4+std::min(i,j)));
 auto deliver=[&](unsigned from,unsigned to,const MembershipEpoch::Effect&e,uint64_t now){
  C(c[from].Send(sockets[from],to,e));auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
  for(;;){std::array<uint8_t,256>b{};Endpoint source{};auto r=sockets[to].Receive(b,source);if(r.kind==ReceiveKind::Packet)return c[to].Receive(source,std::span(b).first(r.size),now);C(r.kind==ReceiveKind::WouldBlock);C(std::chrono::steady_clock::now()<until);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 };
 auto p=c[0].Begin(7,1500,123,0);C(p.send.has_value());C(c[0].Stage(next));C(!c[0].MayAdvance());
 for(unsigned i=1;i<3;++i){C(deliver(0,i,p,1).accepted);C(c[i].Stage(next));C(c[i].Active()==old);}
 MembershipJournal source,candidate;ConfirmedInputStaging::Identity identity{};identity[0]=19;
 std::vector<uint16_t>confirmed(4500,7);C(c[0].OpenJournal(source,2,identity,3,1,confirmed));C(c[2].OpenJournal(candidate,0,identity,3,1));
 auto req=candidate.Request(1);auto resp=source.Receive(at[2],req,1);C(!resp.empty());candidate.Receive(at[0],resp,2);C(!candidate.Failed());
 MembershipJournal denied;C(!c[1].OpenJournal(denied,2,identity,3,1));
 C(c[0].Verify(1500,123,2).accepted);auto ready1=c[1].Verify(1500,123,2);C(ready1.send.has_value()); // deliberately dropped
 C(deliver(2,0,c[2].Verify(1500,123,2),3).accepted);
 auto commit=deliver(1,0,c[1].Tick(251),252);C(commit.apply);C(!commit.send);C(c[0].Active()==old);C(!c[0].Send(sockets[0],1,p));
 commit=c[0].Publish(true);C(commit.send.has_value());C(c[0].Active()==next);
 auto ack1=deliver(0,1,commit,253);C(ack1.apply);C(!ack1.send);C(c[1].Active()==old);ack1=c[1].Publish(true);C(c[1].Active()==next);
 C(deliver(1,0,ack1,254).accepted);
 // Drop first Commit to candidate, then retry the control without loading twice.
 auto ack2=deliver(0,2,c[0].Tick(501),502);C(ack2.apply);ack2=c[2].Publish(true);
 auto start=deliver(2,0,ack2,503);C(start.send.has_value());C(c[0].MayAdvance());
 C(deliver(1,0,deliver(0,1,start,504),505).accepted);
 C(!c[2].MayAdvance()); // first Start to candidate dropped
 auto repeated=c[0].Tick(751);auto started=deliver(0,2,repeated,752);C(c[2].MayAdvance());C(deliver(2,0,started,753).accepted);
 for(auto&peer:c){C(peer.Active()==next);C(peer.Epoch().Epoch()==2);C(peer.MayAdvance());}
 C(!c[0].OpenJournal(denied,2,identity,3,754,confirmed));
 // Wrong endpoint/token do not alter the coordinator.
 auto wire=membership_wire::Encode(0,1,999,105,*start.send);C(!c[1].Receive(at[2],wire,754).accepted);wire[16]^=1;C(!c[1].Receive(at[0],wire,754).accepted);
 // Abort leaves both old active rosters intact, even a proposed leaver.
 auto leave=c[0].Begin(3,3000,456,800);C(c[0].Stage(old));C(deliver(0,1,leave,801).accepted);C(c[1].Stage(old));C(deliver(0,2,leave,801).departed);
 auto abort=c[0].Abort();C(deliver(0,1,abort,802).resumeOld);C(deliver(0,2,abort,802).resumeOld);for(auto&peer:c)C(peer.Active()==next);
 // Failed application emits no ACK and cannot resume or send old controls.
 leave=c[0].Begin(3,3000,456,900);C(c[0].Stage(old));C(deliver(0,1,leave,901).accepted);C(c[1].Stage(old));C(c[0].Verify(3000,456,902).accepted);
 commit=deliver(1,0,c[1].Verify(3000,456,902),903);C(commit.apply);C(c[0].Publish(false).failed);C(c[0].Failed());C(!c[0].MayAdvance());C(!c[0].Send(sockets[0],1,p));C(c[0].Active()==next);
 std::printf("%u channel checks passed; 3 independent Windows UDP sockets\n",checks);
}

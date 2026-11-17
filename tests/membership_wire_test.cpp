#include "udp_transport.h"
#include "membership_wire.h"
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdlib>
using namespace recomp::net;
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
 using K=MembershipEpoch::Kind;using namespace membership_wire;
 MembershipEpoch::Message m{K::Prepare,{1,2,3,1500,0x123456789abcdef0,3,7}};
 auto bytes=Encode(0,2,100,200,m);C(bytes.size()==66);C(bytes[54]==5&&bytes[55]==220);
 auto d=Decode(bytes,0,2,100,200);C(d&&d->proposal==m.proposal&&d->kind==m.kind);
 for(size_t n=0;n<bytes.size();++n)C(!Decode(std::span(bytes).first(n),0,2,100,200));
 auto bad=bytes;bad.push_back(0);C(!Decode(bad,0,2,100,200));
 for(size_t pos:{size_t(0),size_t(4),size_t(5),size_t(6),size_t(7),size_t(8),size_t(16),size_t(32),size_t(40),size_t(64),size_t(65)}){bad=bytes;bad[pos]^=128;C(!Decode(bad,0,2,100,200));}
 C(!Decode(bytes,1,2,100,200));C(!Decode(bytes,0,1,100,200));C(!Decode(bytes,0,2,101,200));C(!Decode(bytes,0,2,100,201));
 C(Encode(1,0,100,200,m).empty());C(Encode(0,0,100,200,m).empty());
 for(auto k:{K::Ready,K::CommitAck,K::Started}){m.kind=k;C(Encode(0,1,100,200,m).empty());C(Decode(Encode(1,0,100,200,m),1,0,100,200).has_value());}
 m.kind=K::Prepare;
 std::array<uint8_t,3>payload{4,5,6};auto game=Game(3,payload);C(GamePayload(game,3).size()==3);C(GamePayload(game,2).empty());C(Game(0,payload).empty());
 ConfirmedInputStaging s;ConfirmedInputStaging::Identity id{};id[0]=42;
 C(!s.Reset(id,1,1500));C(!s.Reset(id,3,UINT64_MAX));C(s.Reset(id,3,1500));
 auto chunk=[](uint64_t first,unsigned frames){std::vector<uint8_t>b;for(unsigned i=0;i<frames;++i)for(unsigned j=0;j<3;++j){uint16_t n=uint16_t((first+i)*17+j+1);b.push_back(uint8_t(n>>8));b.push_back(uint8_t(n));}return b;};
 C(!s.Add(id,UINT64_MAX,chunk(0,1)));C(!s.Add(id,1499,chunk(1499,2)));C(!s.Add(id,0,chunk(0,121)));C(s.Inputs().empty());
 auto wrong=id;wrong[31]=1;C(!s.Add(wrong,0,chunk(0,1)));C(s.Received()==0);
 C(s.Add(id,120,chunk(120,120)));C(s.Add(id,120,chunk(120,120)));C(s.Received()==120);
 auto conflict=chunk(0,121);conflict.back()^=1;C(!s.Add(id,1,conflict));C(s.Received()==120);
 // A legal-size overlap containing new and conflicting existing frames is atomic.
 conflict=chunk(1,120);conflict.back()^=1;C(!s.Add(id,1,conflict));C(s.Received()==120);
 for(int first=1440;first>=0;first-=120)C(s.Add(id,unsigned(first),chunk(unsigned(first),first==1440?60:120)));
 C(s.Complete());C(s.Inputs().size()==4500);
 for(size_t i=0;i<s.Inputs().size();++i)C(s.Inputs()[i]==uint16_t((i/3)*17+i%3+1));
 // Actual OS UDP, known endpoint validation before control decoding.
 Udp host,guest,outsider;std::string error;Endpoint h{},g{},o{},from{};
 C(host.Open(0,error,BindScope::Loopback));C(guest.Open(0,error,BindScope::Loopback));C(outsider.Open(0,error,BindScope::Loopback));
 C(host.Local(h)&&guest.Local(g)&&outsider.Local(o));
 C(outsider.Send(bytes,g));C(host.Send(bytes,g));
 unsigned accepted=0,rejected=0;auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
 while(accepted+rejected<2&&std::chrono::steady_clock::now()<end){std::array<uint8_t,512>b{};auto r=guest.Receive(b,from);if(r.kind==ReceiveKind::WouldBlock){std::this_thread::sleep_for(std::chrono::milliseconds(1));continue;}C(r.kind==ReceiveKind::Packet);if(Same(from,h)&&Decode(std::span(b).first(r.size),0,2,100,200))++accepted;else ++rejected;}
 C(accepted==1&&rejected==1);
 std::printf("%u membership wire/journal checks passed (Windows UDP loopback)\n",checks);
}

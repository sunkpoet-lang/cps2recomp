#include "udp_transport.h"
#include "membership_bootstrap.h"
#include <cstdio>
#include <cstdlib>
using namespace recomp::net;
static unsigned n=0;
#define C(x) do{++n;if(!(x))std::abort();}while(0)
int main(){std::string build(64,'a');std::vector<uint8_t>p{1,2},state{3,4};auto a=MembershipBootstrap(build,p,state);C(a.has_value());char hex[65]{};for(unsigned i=0;i<32;++i)std::sprintf(hex+i*2,"%02x",unsigned((*a)[i]));C(std::string_view(hex)=="88a2b7002ef3fbab7ddf498f32da33e9e9afba764c39ff5db5f224d4303310cc");C(a==MembershipBootstrap(build,p,state));state[1]^=1;C(a!=MembershipBootstrap(build,p,state));state[1]^=1;p[1]^=1;C(a!=MembershipBootstrap(build,p,state));C(!MembershipBootstrap("",p,state));C(!MembershipBootstrap(std::string(64,'z'),p,state));C(!MembershipBootstrap(build,{},state));
 std::vector<uint8_t>record(32);record[0]=12;for(unsigned i=0;i<12;++i)record[4+i]=uint8_t(i+1);std::vector<uint16_t>out{99};C(ExportConfirmedPrefix(record,20,3,2,out));C(out.size()==6&&out[0]==0x0201&&out.back()==0x0c0b);auto old=out;C(!ExportConfirmedPrefix(record,20,3,3,out));C(out==old);C(!ExportConfirmedPrefix(record,20,3,UINT64_MAX,out));C(out==old);std::printf("%u bootstrap/export checks passed\n",n);}


#include "membership_checkpoints.h"
#include <cstdio>
#include <cstdlib>
using namespace recomp::net;
static unsigned checks=0;
#define C(x) do{++checks;if(!(x))std::abort();}while(0)
int main(){MembershipCheckpoints c;std::vector<uint8_t>b(32,7);C(!c.Save(-2,1,b));C(c.Save(-1,1,b));C(c.Find(0)&&c.Find(0)->fingerprint==1);
 for(int i=0;i<200;++i){C(c.Save(i,unsigned(i),b));}C(c.Size()==120);C(c.Bytes()==120*32);C(!c.Find(80));C(c.Find(200));
 C(c.Save(150,999,b));C(c.Find(151)->fingerprint==999);C(!c.Find(152));C(!c.Find(200));C(!c.Save(151,1,{}));C(c.Find(151));c.Clear();C(!c.Size()&&!c.Bytes());
 b.resize(1024*1024);for(int i=0;i<70;++i){C(c.Save(i,1,b));}C(c.Size()==64);C(c.Bytes()==MembershipCheckpoints::maxBytes);c.Clear();
 std::vector<uint8_t>record(20+12,0);record[0]=12;for(unsigned i=0;i<12;++i)record[4+i]=uint8_t(i+1);
 auto input=ConfirmedRecording(record,20,3);C(input.size()==12);C(input[0]==1&&input[11]==12);
 C(ConfirmedRecording(record,19,3).empty());C(ConfirmedRecording(record,20,1).empty());record.back()=1;C(ConfirmedRecording(record,20,3).empty());record.back()=0;record[0]=255;C(ConfirmedRecording(record,20,3).empty());
 std::printf("%u local checkpoint/confirmed recording checks passed\n",checks);
}


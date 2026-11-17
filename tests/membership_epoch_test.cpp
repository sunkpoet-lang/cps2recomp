#include "../port/membership_epoch.h"
#include <cstdio>
#include <cstdlib>
using E=recomp::net::MembershipEpoch;
static int checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static E::Message msg(const E::Effect&e){CHECK(e.send.has_value());return *e.send;}
int main(){
 E h,a,b; CHECK(h.Reset(0,1,3));CHECK(a.Reset(1,1,3));CHECK(b.Reset(2,1,3));
 auto p=msg(h.Begin(7,1500,123,0)); CHECK(!h.MayAdvance());CHECK(!h.Begin(7,1500,123,1).accepted);
 CHECK(!a.Receive(2,p,1).accepted);CHECK(a.Receive(0,p,1).accepted);CHECK(b.Receive(0,p,1).accepted);
 CHECK(!a.VerifyLocal(1499,123,2).accepted);CHECK(!a.VerifyLocal(1500,124,2).accepted);
 CHECK(h.VerifyLocal(1500,123,2).accepted);
 auto ar=msg(a.VerifyLocal(1500,123,2));auto br=msg(b.VerifyLocal(1500,123,2));
 CHECK(h.Receive(1,ar,3).accepted);auto commit=h.Receive(2,br,3);CHECK(commit.apply);CHECK(h.Epoch()==2);
 auto cm=msg(commit);CHECK(!a.MayAdvance());auto ack=msg(a.Receive(0,cm,4));
 CHECK(!a.Receive(0,cm,5).apply);CHECK(!a.AcceptGame(1));CHECK(a.AcceptGame(2));
 auto back=msg(b.Receive(0,cm,4));CHECK(h.Receive(1,ack,5).accepted);
 auto start=msg(h.Receive(2,back,5));CHECK(h.MayAdvance());CHECK(!h.Begin(3,3000,456,6).accepted);
 auto as=msg(a.Receive(0,start,6));CHECK(a.MayAdvance());auto bs=msg(b.Receive(0,start,6));
 CHECK(h.Receive(1,as,7).accepted);CHECK(h.Receive(2,bs,7).accepted);CHECK(!h.Pending());
 CHECK(!h.Receive(1,ar,8).accepted);CHECK(!a.Receive(0,p,8).accepted);
 auto leave=msg(h.Begin(3,3000,456,10));CHECK(a.Receive(0,leave,11).accepted);CHECK(b.Receive(0,leave,11).departed);CHECK(!b.MayAdvance());
 CHECK(h.VerifyLocal(3000,456,12).accepted);auto ready=msg(a.VerifyLocal(3000,456,12));
 auto lc=msg(h.Receive(1,ready,13));auto la=msg(a.Receive(0,lc,14));auto ls=msg(h.Receive(1,la,15));
 CHECK(h.Receive(1,msg(a.Receive(0,ls,16)),17).accepted);CHECK(h.Epoch()==3);CHECK(h.Frame()==3000);
 CHECK(!h.Begin(7,2999,1,18).accepted);CHECK(!h.Begin(0,4000,1,18).accepted);
 // A lost prepare/ready is retried without moving the fixed deadline.
 CHECK(h.Reset(0,1,3,0,1000));CHECK(a.Reset(1,1,3,0,1000));
 p=msg(h.Begin(7,1500,123,0));CHECK(h.Deadline()==1000);CHECK(!h.Tick(249).accepted);CHECK(msg(h.Tick(250)).kind==E::Kind::Prepare);CHECK(h.Deadline()==1000);
 CHECK(a.Receive(0,p,1).accepted);CHECK(a.VerifyLocal(1500,123,2).accepted);CHECK(msg(a.Tick(251)).kind==E::Kind::Ready);
 auto abort=h.Tick(1000);CHECK(abort.resumeOld);CHECK(h.Epoch()==1);CHECK(a.Receive(0,msg(abort),1000).resumeOld);
 auto retry=msg(h.Begin(7,1500,123,1001));CHECK(retry.proposal.id>p.proposal.id);CHECK(!h.Receive(1,ar,1002).accepted);
 CHECK(a.Receive(0,retry,1002).accepted);CHECK(!a.Tick(2001).accepted);CHECK(a.Tick(2002).failed);CHECK(!a.MayAdvance());
 // Commit without Start must fail closed; it cannot return to the old epoch.
 CHECK(h.Reset(0,1,3,0,1000));CHECK(a.Reset(1,1,3,0,1000));
 p=msg(h.Begin(1,1500,123,0));CHECK(a.Receive(0,p,1).departed);CHECK(h.VerifyLocal(1500,123,2).apply);CHECK(h.MayAdvance());CHECK(h.Members()==1);
 CHECK(h.Reset(0,1,1,0,1000));CHECK(a.Reset(1,1,1,0,1000));
 p=msg(h.Begin(3,1500,123,0));CHECK(a.Receive(0,p,1).accepted);CHECK(h.VerifyLocal(1500,123,2).accepted);
 cm=msg(h.Receive(1,msg(a.VerifyLocal(1500,123,2)),3));CHECK(a.Receive(0,cm,4).apply);
 CHECK(h.Tick(1000).failed);CHECK(a.Tick(1001).failed);CHECK(h.Epoch()==2);CHECK(a.Epoch()==2);CHECK(!a.AcceptGame(2));CHECK(!h.Abort().accepted);
 CHECK(h.Reset(0,1,3));CHECK(a.Reset(1,1,3));p=msg(h.Begin(1,1500,123,0));
 CHECK(a.Receive(0,p,1).departed);CHECK(!a.MayAdvance());CHECK(a.Receive(0,msg(h.Abort()),2).resumeOld);CHECK(a.MayAdvance());
 CHECK(!h.Reset(4,1,3));CHECK(!h.Reset(0,0,3));CHECK(!h.Reset(0,1,2));CHECK(!h.Reset(0,1,17));
 CHECK(h.Reset(0,UINT64_MAX,3));CHECK(!h.Begin(7,1500,123,0).accepted);
 CHECK(h.Reset(0,1,3,0,1000,UINT64_MAX));CHECK(!h.Begin(7,1500,123,0).accepted);
 std::printf("%d membership checks passed\n",checks);
}

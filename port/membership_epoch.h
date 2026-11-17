// SPDX-License-Identifier: GPL-3.0-only
// Pure quorum control. Checkpoint validation/loading and transport belong to callers.
#pragma once
#include <bit>
#include <cstdint>
#include <limits>
#include <optional>
namespace recomp::net {
class MembershipEpoch {
public:
    enum class Stage { Running, Preparing, Committing, AwaitStart, Failed, Departed };
    enum class Kind { Prepare, Ready, Commit, CommitAck, Start, Started, Abort };
    struct Proposal {
        uint64_t id=0, oldEpoch=0, nextEpoch=0, frame=0, fingerprint=0;
        uint8_t oldMembers=0, newMembers=0;
        bool operator==(const Proposal&)const=default;
    };
    struct Message {Kind kind;Proposal proposal;};
    struct Effect {
        bool accepted=false, apply=false, resumeOld=false, departed=false, failed=false;
        std::optional<Message> send;
    };
private:
    bool valid_=false,have_=false,verified_=false,applied_=false;
    unsigned self_=0;uint8_t members_=0,ready_=0,acks_=0,started_=0;
    uint64_t epoch_=1,frame_=0,lastId_=0,timeout_=10000,deadline_=0,nextRetry_=0;
    Stage stage_=Stage::Failed;Proposal proposal_{};
    static uint64_t Add(uint64_t now,uint64_t delay){return delay>std::numeric_limits<uint64_t>::max()-now?std::numeric_limits<uint64_t>::max():now+delay;}
    uint8_t Bit()const{return uint8_t(1u<<self_);}
    static bool Mask(uint8_t mask){return (mask&1)&&!(mask&~15u);}
    Effect Emit(Kind kind,bool apply=false)const{Effect e;e.accepted=true;e.apply=apply;e.send=Message{kind,proposal_};return e;}
    Effect Accepted()const{Effect e;e.accepted=true;return e;}
    bool Member(unsigned peer)const{return peer<4&&(proposal_.newMembers&(1u<<peer));}
    bool ValidProposal(const Proposal&p)const{
        return p.id>lastId_&&epoch_!=std::numeric_limits<uint64_t>::max()&&p.oldEpoch==epoch_&&p.nextEpoch==epoch_+1&&p.oldMembers==members_&&Mask(p.newMembers)&&std::popcount(unsigned(p.oldMembers^p.newMembers))==1&&p.frame>=frame_;
    }
    void Adopt(const Proposal&p,uint64_t now){proposal_=p;have_=true;lastId_=p.id;verified_=applied_=false;ready_=acks_=started_=0;stage_=Stage::Preparing;deadline_=Add(now,timeout_);nextRetry_=Add(now,250);}
    void Apply(){epoch_=proposal_.nextEpoch;members_=proposal_.newMembers;frame_=proposal_.frame;applied_=true;}
    Effect Commit(){
        Apply();stage_=Stage::Committing;acks_=started_=Bit();
        if(acks_==members_){stage_=Stage::Running;deadline_=0;auto e=Accepted();e.apply=true;return e;}
        return Emit(Kind::Commit,true);
    }
    Effect AbortHost(){stage_=Stage::Running;deadline_=0;verified_=false;auto e=Emit(Kind::Abort);e.resumeOld=true;return e;}
    Effect Expire(uint64_t now){
        if(!deadline_||now<deadline_)return {};
        if(self_==0&&stage_==Stage::Preparing)return AbortHost();
        stage_=Stage::Failed;deadline_=0;Effect e;e.accepted=e.failed=true;return e;
    }
public:
    bool Reset(unsigned self,uint64_t epoch,uint8_t members,uint64_t stableFrame=0,uint64_t timeout=10000,uint64_t lastId=0){
        valid_=self<4&&epoch&&Mask(members)&&timeout;self_=self<4?self:0;epoch_=epoch;members_=members;frame_=stableFrame;timeout_=timeout;lastId_=lastId;
        have_=verified_=applied_=false;deadline_=nextRetry_=0;ready_=acks_=started_=0;stage_=valid_?Stage::Running:Stage::Failed;return valid_;
    }
    uint64_t Epoch()const{return epoch_;}uint64_t Frame()const{return frame_;}uint8_t Members()const{return members_;}uint64_t Deadline()const{return deadline_;}Stage State()const{return stage_;}
    bool MayAdvance()const{return valid_&&stage_==Stage::Running&&(members_&Bit());}
    bool AcceptGame(uint64_t epoch)const{return valid_&&epoch==epoch_&&(members_&Bit())&&stage_!=Stage::Failed&&stage_!=Stage::Departed;}
    bool Pending()const{return deadline_!=0;}const Proposal&Current()const{return proposal_;}
    Effect Begin(uint8_t nextMembers,uint64_t checkpoint,uint64_t fingerprint,uint64_t now){
        if(auto e=Expire(now);e.accepted)return e;
        if(!valid_||self_!=0||stage_!=Stage::Running||deadline_||lastId_==std::numeric_limits<uint64_t>::max())return {};
        Proposal p{lastId_+1,epoch_,epoch_==std::numeric_limits<uint64_t>::max()?0:epoch_+1,checkpoint,fingerprint,members_,nextMembers};
        if(!ValidProposal(p))return {};
        Adopt(p,now);return Emit(Kind::Prepare);
    }
    Effect VerifyLocal(uint64_t checkpoint,uint64_t fingerprint,uint64_t now){
        if(auto e=Expire(now);e.accepted)return e;
        if(!valid_||!have_||stage_!=Stage::Preparing||checkpoint!=proposal_.frame||fingerprint!=proposal_.fingerprint||!Member(self_))return {};
        verified_=true;
        if(self_!=0)return Emit(Kind::Ready);
        ready_|=Bit();return ready_==proposal_.newMembers?Commit():Accepted();
    }
    Effect Abort(){if(valid_&&self_==0&&stage_==Stage::Preparing)return AbortHost();return {};}
    Effect Receive(unsigned sender,const Message&m,uint64_t now){
        if(!valid_||sender>=4||sender==self_)return {};
        if(auto e=Expire(now);e.accepted)return e;
        if(m.kind==Kind::Prepare){
            if(sender!=0||self_==0)return {};
            if(have_&&m.proposal==proposal_&&stage_==Stage::Preparing)return verified_?Emit(Kind::Ready):Accepted();
            if(stage_!=Stage::Running||deadline_||!ValidProposal(m.proposal))return {};
            if(!(m.proposal.newMembers&Bit())){
                if(!(members_&Bit()))return {};
                proposal_=m.proposal;have_=true;lastId_=proposal_.id;verified_=applied_=false;stage_=Stage::Departed;Effect e;e.accepted=e.departed=true;return e;
            }
            Adopt(m.proposal,now);return Accepted();
        }
        if(!have_||m.proposal!=proposal_)return {};
        if(self_==0){
            if(!Member(sender))return {};
            if(m.kind==Kind::Ready){
                if(stage_==Stage::Preparing){ready_|=uint8_t(1u<<sender);return verified_&&ready_==proposal_.newMembers?Commit():Accepted();}
                if(stage_==Stage::Committing)return Emit(Kind::Commit);
            }
            if(m.kind==Kind::CommitAck){
                if(stage_==Stage::Committing){acks_|=uint8_t(1u<<sender);if(acks_==members_){stage_=Stage::Running;return Emit(Kind::Start);}return Accepted();}
                if(stage_==Stage::Running&&applied_)return Emit(Kind::Start);
            }
            if(m.kind==Kind::Started&&stage_==Stage::Running&&applied_){started_|=uint8_t(1u<<sender);if(started_==members_)deadline_=0;return Accepted();}
            return {};
        }
        if(sender!=0)return {};
        if(m.kind==Kind::Commit){
            if(stage_==Stage::Preparing&&verified_){Apply();stage_=Stage::AwaitStart;return Emit(Kind::CommitAck,true);}
            if(applied_&&(stage_==Stage::AwaitStart||stage_==Stage::Running))return Emit(Kind::CommitAck);
        }
        if(m.kind==Kind::Start&&applied_&&(stage_==Stage::AwaitStart||stage_==Stage::Running)){stage_=Stage::Running;deadline_=0;return Emit(Kind::Started);}
        if(m.kind==Kind::Abort&&!applied_&&epoch_==proposal_.oldEpoch&&(stage_==Stage::Preparing||stage_==Stage::Failed||stage_==Stage::Departed)){
            deadline_=0;verified_=false;auto e=Accepted();
            if(members_&Bit()){stage_=Stage::Running;e.resumeOld=true;}else{stage_=Stage::Departed;e.departed=true;}
            return e;
        }
        return {};
    }
    Effect Tick(uint64_t now){
        if(!valid_)return {};
        if(auto e=Expire(now);e.accepted)return e;
        if(!deadline_||now<nextRetry_)return {};
        nextRetry_=Add(now,250);
        if(self_==0){if(stage_==Stage::Preparing)return Emit(Kind::Prepare);if(stage_==Stage::Committing)return Emit(Kind::Commit);if(stage_==Stage::Running)return Emit(Kind::Start);}
        else{if(stage_==Stage::Preparing&&verified_)return Emit(Kind::Ready);if(stage_==Stage::AwaitStart)return Emit(Kind::CommitAck);}
        return {};
    }
};
}

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 the fork's contributors.
#include "rollback_session.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

int main() {
    using namespace recomp::rollback;
    unsigned checks = 0;
    auto check = [&](bool ok) { if (!ok) throw std::runtime_error("rollback kernel check " + std::to_string(checks)); ++checks; };
    for (unsigned players = 2; players <= 4; ++players) {
        std::vector<uint8_t> scratch;
        std::array<uint8_t,16> saved{};
        std::array<uint8_t,8> inputs{1,0,2,1,4,0,8,1};
        unsigned length = 0, hash = 0;
        bool active = true, rejectSave = false, rejectLoad = false, rejectAdvance = false;
        int errors = 0; Error last = Error::Event; std::string order;
        Callbacks cb;
        cb.active = [&] { return active; };
        cb.save = [&](int frame, auto& bytes) { check(frame == 7); order += 'S'; bytes = {9,8,7}; return !rejectSave; };
        cb.checksum = [&](int frame) { check(frame == 7 && length == 3 && saved[0] == 9); order += 'C'; return 1234u; };
        cb.load = [&](int frame, auto bytes) { check(frame == 7 && bytes.size() == 3 && bytes[2] == 7); order += 'L'; return !rejectLoad; };
        cb.advance = [&](Frame frame, auto bytes) { check(frame.number == 8 && frame.rollback && !frame.runahead && bytes.size() == players * 2 && bytes[0] == 1); order += 'A'; return !rejectAdvance; };
        cb.error = [&](Error error) { ++errors; last = error; };
        GekkoGameEvent save{}; save.type=GekkoSaveEvent; save.data.save={7,false,&hash,&length,saved.data()};
        GekkoGameEvent load{}; load.type=GekkoLoadEvent; load.data.load={7,3,saved.data()};
        GekkoGameEvent advance{}; advance.type=GekkoAdvanceEvent; advance.data.adv={8,players*2,inputs.data(),true,false};
        std::array<GekkoGameEvent*,3> events{&save,&load,&advance};
        check(Dispatch(events.data(),3,{players,2,16},scratch,cb));check(order=="SCLA" && hash==1234 && !errors);
        order.clear(); active=false; check(!Dispatch(events.data(),3,{players,2,16},scratch,cb));check(order.empty());active=true;
        check(!Dispatch(events.data(),3,{players,2,2},scratch,cb));check(last==Error::StateCapacity);order.clear();
        auto badSave=save;badSave.data.save.state=nullptr;GekkoGameEvent* single=&badSave;check(!Dispatch(&single,1,{players,2,16},scratch,cb));check(last==Error::SaveBuffer&&order.empty());
        for(auto size:{0u,players*2-1,players*2+1}){auto bad=advance;bad.data.adv.input_len=size;single=&bad;check(!Dispatch(&single,1,{players,2,16},scratch,cb));check(last==Error::InputBuffer);}
        auto bad=advance;bad.data.adv.inputs=nullptr;single=&bad;check(!Dispatch(&single,1,{players,2,16},scratch,cb));check(last==Error::InputBuffer);
        auto badLoad=load;badLoad.data.load.state_len=17;single=&badLoad;check(!Dispatch(&single,1,{players,2,16},scratch,cb));check(last==Error::LoadBuffer);
        badLoad=load;badLoad.data.load.state=nullptr;single=&badLoad;check(!Dispatch(&single,1,{players,2,16},scratch,cb));check(last==Error::LoadBuffer);
        check(!Dispatch(nullptr,1,{players,2,16},scratch,cb));check(last==Error::Configuration);
        single=nullptr;check(!Dispatch(&single,1,{players,2,16},scratch,cb));check(last==Error::Event);
        for(unsigned count:{0u,1u,5u})check(!Dispatch(events.data(),3,{count,2,16},scratch,cb));
        rejectSave=true;order.clear();check(!Dispatch(events.data(),3,{players,2,16},scratch,cb));check(order=="S");rejectSave=false;
        rejectLoad=true;order.clear();check(!Dispatch(events.data(),3,{players,2,16},scratch,cb));check(order=="SCL");rejectLoad=false;
        rejectAdvance=true;order.clear();check(!Dispatch(events.data(),3,{players,2,16},scratch,cb));check(order=="SCLA");rejectAdvance=false;
        cb.advance=[&](Frame frame,auto bytes){check(frame.runahead&&!frame.rollback&&bytes.size()==players*2);return true;};advance.data.adv.rolling_back=false;advance.data.adv.running_ahead=true;single=&advance;check(Dispatch(&single,1,{players,2,16},scratch,cb));
        cb.save=[&](int,auto&bytes){bytes.assign(16,0x5a);return true;};cb.checksum=[](int){return 7u;};single=&save;check(Dispatch(&single,1,{players,2,16},scratch,cb));check(length==16&&saved[15]==0x5a);
    }
    std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"player_counts\":[2,3,4]}\n";
}

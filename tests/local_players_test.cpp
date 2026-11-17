// SPDX-License-Identifier: GPL-3.0-only
#include "player_inputs.h"
#include <iostream>
#include <stdexcept>
using namespace recomp::rollback;
int main(){unsigned checks=0;auto check=[&](bool b){++checks;if(!b)throw std::runtime_error("local input check "+std::to_string(checks));};
 std::array<uint16_t,2>local{0x1234,0xabcd};Pads out{};
 check(LocalInputs(local,3,4,out)&&out==Pads({0x1234,0xabcd,0,0}));
 check(LocalInputs(local,12,4,out)&&out==Pads({0,0,0x1234,0xabcd}));
 check(LocalInputs(local,6,3,out)&&out==Pads({0,0x1234,0xabcd,0}));
 check(LocalInputs(std::span(local.data(),1),4,3,out)&&out==Pads({0,0,0x1234,0}));
 check(!LocalInputs(std::span(local.data(),1),12,4,out)&&out==Pads{});
 check(!LocalInputs(local,0,4,out)&&out==Pads{});check(!LocalInputs(local,16,4,out)&&out==Pads{});
 check(!LocalInputs(local,7,4,out)&&out==Pads{});check(!LocalInputs(local,12,3,out)&&out==Pads{});
 check(!LocalInputs(local,3,1,out)&&out==Pads{});check(!LocalInputs(local,3,5,out)&&out==Pads{});
 check(!LocalInputs({},3,4,out)&&out==Pads{});
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"physical_to_seat_mapping\":true}\n";
}

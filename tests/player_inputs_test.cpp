// SPDX-License-Identifier: GPL-3.0-only
#include "player_inputs.h"
#include "replay.h"
#include <iostream>
#include <stdexcept>
using namespace recomp::rollback;
static unsigned checks=0;
static void check(bool value){++checks;if(!value)throw std::runtime_error("check "+std::to_string(checks));}
int main(){
 for(size_t n=2;n<=4;++n){
  Pads input{0x1234,0xabcd,0x0801,0x4002},decoded{};std::span<const uint8_t>bytes(reinterpret_cast<const uint8_t*>(input.data()),n*sizeof(uint16_t));
  check(DecodeInputs(bytes,n,decoded));for(size_t i=0;i<4;++i)check(decoded[i]==(i<n?input[i]:0));
  Pads board{9,9,9,9};check(BoardInputs(std::span(input.data(),n),board));check(board==decoded);
  check(!DecodeInputs(bytes.first(bytes.size()-1),n,decoded));check(decoded==Pads{});
  check(!DecodeInputs(bytes,1,decoded)&&decoded==Pads{});check(!DecodeInputs(bytes,5,decoded)&&decoded==Pads{});
  check(!BoardInputs(std::span(input.data(),1),board)&&board==Pads{});check(!BoardInputs(input,std::span(board.data(),n-1)));
  port::Replay replay;replay.players=int(n);replay.info.game="fixture";replay.info.frames=120;replay.info.players.assign(n,"Player");replay.state={1,2,3};
  for(size_t i=n;i<4;++i)input[i]=0;replay.inputs[0]=input;input[n-1]^=0x4000;replay.inputs[60]=input;replay.checks={{60,0x12345678},{120,0xabcdef01}};
  auto data=replay.Bytes();check(data[8]==1&&data[9]==0);port::Replay loaded;std::string error;check(loaded.Read(data,error));check(loaded.players==int(n)&&loaded.info.players.size()==n);check(loaded.inputs==replay.inputs&&loaded.checks==replay.checks&&loaded.state==replay.state);
 }
 Pads output{};check(!DecodeInputs({},0,output)&&output==Pads{});check(!BoardInputs({},output)&&output==Pads{});
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"players\":[2,3,4],\"replay_format\":1}\n";
}

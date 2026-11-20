#include <cassert>
#include <cstdio>
#include <vector>
#include "../firmware/bytereel_2026/assembler.h"
#include "../firmware/bytereel_2026/vm.h"

int main(){
  const char*src="FREQ 18\nWAVE 00\nATTACK 00\nDECAY 00\nGATE 01\nAGAIN:\nVOLUME 40\nWAIT FA\nVOLUME 80\nWAIT FA\nVOLUME C0\nWAIT FA\nJMP AGAIN";
  BrAsmResult image=br_assemble(src,"AUDIO");assert(image.written==24);
  VM vm;uint8_t video[256]={},audio[256];memcpy(audio,image.bytes,256);vm.reset(VM::AUDIO);vm.start(VM::AUDIO);
  std::vector<uint8_t> levels;uint32_t now=0,fetchSeen=vm.fetchSerial;uint32_t generation=vm.synthGeneration;
  for(unsigned i=0;i<100&&levels.size()<4;i++){now+=3000;vm.tick(now,video,audio);if(vm.fetchSerial!=fetchSeen){fetchSeen=vm.fetchSerial;if(vm.lastFetchOp==0x12)levels.push_back((uint8_t)vm.volume);}assert(vm.freq==440||vm.freq==200);assert(vm.wave==0);assert(vm.gate==1);assert(vm.synthGeneration==generation);}
  assert((levels==std::vector<uint8_t>{0x40,0x80,0xc0,0x40}));
  std::puts("PASS 440 Hz diagnostic program: fixed sine/gate/phase, 40/80/C0 levels, repeat without envelope retrigger");
}

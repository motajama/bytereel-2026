#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include "../firmware/bytereel_2026/vm.h"
#include "../firmware/bytereel_2026/audio_synth.h"

int main(){
  VM vm;vm.reset(VM::AUDIO);vm.freq=440;vm.wave=0;vm.volume=255;vm.filter=255;vm.gate=1;vm.attack=0;vm.decay=0;
  AudioSynth synth;synth.reset();int16_t first[128],second[128];synth.render(vm,first,128);synth.render(vm,second,128);
  int maxStep=0;uint8_t previous=0;bool havePrevious=false;
  for(const int16_t sample: first){const uint16_t word=(uint16_t)sample;const uint8_t dac=(uint8_t)(word>>8);assert((word&0xff)==0);assert(dac>=32&&dac<=224);if(havePrevious){int d=(int)dac-previous;if(d<0)d=-d;if(d>maxStep)maxStep=d;}previous=dac;havePrevious=true;}
  const uint8_t boundary=(uint8_t)((uint16_t)first[127]>>8),next=(uint8_t)((uint16_t)second[0]>>8);int jump=(int)next-boundary;if(jump<0)jump=-jump;assert(jump<20);
  for(const int16_t sample: second){const uint16_t word=(uint16_t)sample;const uint8_t dac=(uint8_t)(word>>8);assert((word&0xff)==0);assert(dac>=32&&dac<=224);int d=(int)dac-previous;if(d<0)d=-d;if(d>maxStep)maxStep=d;previous=dac;}
  assert(maxStep<20);
  std::puts("PASS audio renderer: DAC lane/alignment, bounded headroom, sine continuity across 128-sample blocks");
}

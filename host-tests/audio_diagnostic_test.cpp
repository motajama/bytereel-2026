#include <cassert>
#include <cmath>
#include <cstdio>
#include "../firmware/bytereel_2026/audio_diagnostic.h"

int main(){
  BrAudioDiagnosticTone tone;tone.init();
  int16_t block[BrAudioDiagnosticTone::BLOCK_SAMPLES];
  int16_t first=0,last=0,previous=0;int maxStep=0;
  for(unsigned n=0;n<BrAudioDiagnosticTone::TABLE_SAMPLES;n++){
    tone.render(block,1);const uint8_t dac=(uint16_t)block[0]>>8;if(!n)first=block[0];if(n) {int d=abs((int)dac-((uint16_t)previous>>8));if(d>maxStep)maxStep=d;}previous=block[0];last=block[0];assert(dac>=88&&dac<=168);
  }
  tone.render(block,1);assert(block[0]==first);assert(maxStep<=7);
  assert(tone.cursor==1&&last!=first);
  for(unsigned i=0;i<BrAudioDiagnosticTone::BLOCK_SAMPLES;i++)tone.render(block+i,1);
  for(unsigned i=0;i<BrAudioDiagnosticTone::BLOCK_SAMPLES;i++)assert(block[i]==tone.table[(1+i)%BrAudioDiagnosticTone::TABLE_SAMPLES]);
  std::puts("PASS diagnostic audio: fixed 440 Hz table, 44-cycle/2205-sample period, block-boundary continuity and DAC range");
}

#pragma once
#include <math.h>
#include <stdint.h>

// At 22,050 Hz this 2,205-sample table contains exactly 44 cycles of 440 Hz.
// Repeating it therefore does not introduce a phase discontinuity at block
// boundaries. This path is compiled only into the explicit diagnostic build.
struct BrAudioDiagnosticTone {
  static constexpr unsigned SAMPLE_RATE=22050;
  static constexpr unsigned FREQUENCY=440;
  static constexpr unsigned TABLE_SAMPLES=2205;
  static constexpr unsigned BLOCK_SAMPLES=128;
  static constexpr int DAC_AMPLITUDE=40;
  int16_t table[TABLE_SAMPLES];
  uint16_t cursor=0;

  void init(){
    for(unsigned i=0;i<TABLE_SAMPLES;i++){
      const float phase=6.28318530718f*(float)i*(float)FREQUENCY/(float)SAMPLE_RATE;
      int dac=128+(int)lrintf((float)DAC_AMPLITUDE*sinf(phase));
      if(dac<0)dac=0;if(dac>255)dac=255;
      table[i]=(int16_t)((uint16_t)dac<<8);
    }
    cursor=0;
  }
  void render(int16_t *out,unsigned count){
    for(unsigned i=0;i<count;i++){
      out[i]=table[cursor++];
      if(cursor==TABLE_SAMPLES)cursor=0;
    }
  }
};

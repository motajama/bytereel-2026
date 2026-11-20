#pragma once
#include <stdint.h>
#include <math.h>
#include "vm.h"
struct AudioSynth {
  float phase=0,lfoPhase=0,env=1,filterState=0;uint32_t rng=0x6d2b79f5u;uint8_t priorGate=1;
  uint32_t random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
  float white(){return (float)(random()>>8)*(2.0f/16777215.0f)-1.0f;}
  void reset(){phase=lfoPhase=filterState=0;env=1;priorGate=1;rng=0x6d2b79f5u;}
  void render(const VM &v,int16_t *out,unsigned count){
    // VM instructions run on the other core. Snapshot each scalar once so a
    // block never changes oscillator/filter/envelope settings halfway through.
    const uint16_t frequency=v.freq;const uint8_t waveform=v.wave,volumeValue=v.volume,attackValue=v.attack,decayValue=v.decay,gateValue=v.gate,pulseValue=v.pulse,crushValue=v.crush,noiseValue=v.noise,lfoRateValue=v.lfoRate,lfoDepthValue=v.lfoDepth,filterValue=v.filter;
    const float sr=22050.0f;float volume=volumeValue/255.0f,mix=noiseValue/255.0f;
    float attackMs=attackValue*4.0f,decayMs=decayValue*4.0f;
    float lfoHz=lfoRateValue?0.1f+0.05f*(lfoRateValue-1):0.0f,depth=lfoDepthValue/255.0f;
    float fc=100.0f+5900.0f*filterValue/255.0f,alpha=1.0f-expf(-6.28318530718f*fc/sr);
    float pulse=pulseValue/255.0f;if(pulse<0.01f)pulse=0.01f;if(pulse>0.99f)pulse=0.99f;
    unsigned bits=(crushValue==0||crushValue>=8)?8:crushValue;float levels=(float)((1u<<bits)-1u);
    if(gateValue&&!priorGate)env=0;priorGate=gateValue;
    for(unsigned i=0;i<count;i++){
      float target=gateValue?1.0f:0.0f,ms=gateValue?attackMs:decayMs;
      if(ms<=0)env=target;else{float d=1.0f/(sr*ms/1000.0f);env+=target>env?d:-d;if(env<0)env=0;if(env>1)env=1;}
      float lfo=sinf(lfoPhase*6.28318530718f);lfoPhase+=lfoHz/sr;if(lfoPhase>=1)lfoPhase-=floorf(lfoPhase);
      float hz=frequency*(1.0f+lfo*depth);if(hz<10)hz=10;if(hz>sr*0.45f)hz=sr*0.45f;
      phase+=hz/sr;if(phase>=1)phase-=floorf(phase);
      float wave=0;switch(waveform){case 0:wave=sinf(phase*6.28318530718f);break;case 1:wave=2*phase-1;break;case 2:wave=phase<.5f?1:-1;break;case 3:wave=1-4*fabsf(phase-.5f);break;case 4:wave=white();break;case 5:wave=phase<pulse?1:-1;break;default:break;}
      wave=wave*(1-mix)+white()*mix;if(bits<8)wave=roundf(((wave+1)*.5f)*levels)/levels*2-1;
      float sample=wave*volume*env*96.0f;filterState+=alpha*(sample-filterState);if(filterState>96)filterState=96;if(filterState< -96)filterState=-96;
      uint16_t dac=(uint16_t)(128.0f+filterState);out[i]=(int16_t)(dac<<8);
    }
  }
};

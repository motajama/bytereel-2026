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
    const float sr=22050.0f;float volume=v.volume/255.0f, mix=v.noise/255.0f;
    float attackMs=v.attack*4.0f,decayMs=v.decay*4.0f;
    float lfoHz=v.lfoRate?0.1f+0.05f*(v.lfoRate-1):0.0f,depth=v.lfoDepth/255.0f;
    float fc=100.0f+5900.0f*v.filter/255.0f,alpha=1.0f-expf(-6.28318530718f*fc/sr);
    float pulse=v.pulse/255.0f;if(pulse<0.01f)pulse=0.01f;if(pulse>0.99f)pulse=0.99f;
    unsigned bits=(v.crush==0||v.crush>=8)?8:v.crush;float levels=(float)((1u<<bits)-1u);
    if(v.gate&&!priorGate)env=0;priorGate=v.gate;
    for(unsigned i=0;i<count;i++){
      float target=v.gate?1.0f:0.0f,ms=v.gate?attackMs:decayMs;
      if(ms<=0)env=target;else{float d=1.0f/(sr*ms/1000.0f);env+=target>env?d:-d;if(env<0)env=0;if(env>1)env=1;}
      float lfo=sinf(lfoPhase*6.28318530718f);lfoPhase+=lfoHz/sr;if(lfoPhase>=1)lfoPhase-=floorf(lfoPhase);
      float hz=v.freq*(1.0f+lfo*depth);if(hz<10)hz=10;if(hz>sr*0.45f)hz=sr*0.45f;
      phase+=hz/sr;if(phase>=1)phase-=floorf(phase);
      float wave=0;switch(v.wave){case 0:wave=sinf(phase*6.28318530718f);break;case 1:wave=2*phase-1;break;case 2:wave=phase<.5f?1:-1;break;case 3:wave=1-4*fabsf(phase-.5f);break;case 4:wave=white();break;case 5:wave=phase<pulse?1:-1;break;default:break;}
      wave=wave*(1-mix)+white()*mix;if(bits<8)wave=roundf(((wave+1)*.5f)*levels)/levels*2-1;
      float sample=wave*volume*env*96.0f;filterState+=alpha*(sample-filterState);if(filterState>96)filterState=96;if(filterState< -96)filterState=-96;
      uint16_t dac=(uint16_t)(128.0f+filterState);out[i]=(int16_t)(dac<<8);
    }
  }
};

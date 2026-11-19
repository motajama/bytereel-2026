#pragma once
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "vm.h"
static uint32_t br_video_random(VM &vm){vm.videoRng^=vm.videoRng<<13;vm.videoRng^=vm.videoRng>>17;vm.videoRng^=vm.videoRng<<5;return vm.videoRng;}
static uint8_t br_gray(uint8_t x){return (uint8_t)((((uint16_t)x*7+127)/255)<<5)|((uint8_t)(((uint16_t)x*7+127)/255)<<2)|(uint8_t)(((uint16_t)x*3+127)/255);}
static void br_render_video(VM &vm,uint8_t **lines,uint8_t *history,uint32_t frame){
  if(!lines)return;const int w=256,h=192;int64_t motion16=(int64_t)vm.motion*(uint64_t)frame;int dxMotion=(int)(motion16/16%w);float blend=vm.feedback/255.0f;int den=vm.density?vm.density*4:1;int sc=vm.scale?vm.scale:1;
  int bandY=-1,bandH=0,bandDx=0;uint8_t prob=vm.glitch&15,amp=vm.glitch>>4;
  if(prob&&((br_video_random(vm)&15)<prob)){bandY=br_video_random(vm)%h;bandH=4+br_video_random(vm)%25;bandDx=(int)(br_video_random(vm)%(amp*2+1))-amp;}
  for(int y=0;y<240;y++){
    if(y<24||y>=216){memset(lines[y],0,256);continue;}
    const int gy=y-24;for(int x=0;x<w;x++){
      int xx=(x+vm.shift+dxMotion+((gy>=bandY&&gy<bandY+bandH)?bandDx:0));
      float value=0;uint32_t q;
      switch(vm.pattern){
        case 0:value=0;break;
        case 1:value=(br_video_random(vm)&1)?255:0;break;
        case 2:value=(((2*((gy*den)%h))/h)&1)?255:0;break;
        case 3:{int xmod=xx%w;if(xmod<0)xmod+=w;value=(((2*((xmod*den)%w))/w)&1)?255:0;break;}
        case 4:value=(((xx/(18*8/(vm.density?vm.density:1)))+(gy/(18*8/(vm.density?vm.density:1))))&1)?255:0;break;
        case 5:{int cellx=xx/(38-(vm.density/8)),celly=gy/(38-(vm.density/8));q=(uint32_t)(cellx*12347)^(uint32_t)(celly*65419)^vm.videoSeed;value=((q>>((x+gy)&7))&1)?255:0;break;}
        case 6:value=(((xx/sc)^(gy/sc)^vm.videoSeed)&(256/den?256/den:1))?255:0;break;
        case 7:{int cx=xx/8,cy=gy/8;q=(uint32_t)(cx*173)+(uint32_t)(cy*97)+vm.videoSeed*17;value=(q^(q>>3)^(q>>6))&1?255:0;break;}
        case 8:{int n=(xx+gy)*den;int rem=n%(w+h);if(rem<0)rem+=w+h;value=(((2*rem)/(w+h))&1)?255:0;break;}
      }
      if(vm.invert)value=255-value;value=value*vm.level/255.0f;int pos=gy*w+x;
      if(blend>0)value=value*(1.0f-blend)+history[pos]*blend;uint8_t out=(uint8_t)value;history[pos]=out;lines[y][x]=br_gray(out);
    }
  }
}

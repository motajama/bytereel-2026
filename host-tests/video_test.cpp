#include <cassert>
#include <cstdio>
#include "../firmware/bytereel_2026/video_renderer.h"
static uint8_t fb[240][256],hist[256*192];static uint8_t *lines[240];
int main(){for(int y=0;y<240;y++)lines[y]=fb[y];VM vm;vm.reset(VM::VIDEO);br_render_video(vm,lines,hist,0);assert(fb[0][0]==0&&fb[239][255]==0&&hist[0]==0);vm.pattern=1;vm.level=255;vm.rng=0x12345678;vm.videoRng=0x9e3779b9;br_render_video(vm,lines,hist,1);bool lo=false,hi=false;for(int y=24;y<216;y++)for(int x=0;x<256;x++){uint8_t v=hist[(y-24)*256+x];lo|=v==0;hi|=v==255;}assert(lo&&hi);vm.feedback=255;for(int i=0;i<256*192;i++)hist[i]=77;vm.pattern=0;br_render_video(vm,lines,hist,2);assert(hist[100]==77);std::puts("PASS video renderer: margins, generated noise, full feedback retention");}

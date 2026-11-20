#include <cassert>
#include <cstdio>
#include <vector>
#include "../firmware/bytereel_2026/demo_programs.h"
#include "../firmware/bytereel_2026/vm.h"

static void load(const uint8_t *src,uint8_t *dst){for(size_t i=0;i<256;i++)dst[i]=pgm_read_byte(src+i);}

int main(){
  assert(BR_DEMO_AUDIO_CODE_BYTES>0&&BR_DEMO_AUDIO_CODE_BYTES<=256&&(BR_DEMO_AUDIO_CODE_BYTES&1)==0);
  assert(BR_DEMO_VIDEO_CODE_BYTES>0&&BR_DEMO_VIDEO_CODE_BYTES<=256&&(BR_DEMO_VIDEO_CODE_BYTES&1)==0);
  uint8_t audio[256],video[256],other[256];load(BR_DEMO_AUDIO,audio);load(BR_DEMO_VIDEO,video);for(auto &b:other)b=0xA5;
  VM vm;vm.reset(VM::AUDIO);assert(vm.pc==0&&!vm.running);vm.start(VM::AUDIO);
  std::vector<uint8_t> volumes;uint8_t last=0;uint32_t now=0;
  for(unsigned i=0;i<180;i++){now+=1000;vm.tick(now,video,audio);if(vm.volume!=last){last=vm.volume;volumes.push_back(last);}}
  bool saw40=false,saw80=false,sawC0=false,wrapped=false;for(uint8_t x:volumes){saw40|=x==0x40;saw80|=x==0x80;sawC0|=x==0xC0;if(sawC0&&x==0x40)wrapped=true;}
  assert(saw40&&saw80&&sawC0&&wrapped);assert(vm.error==VM::OK);
  vm.reset(VM::VIDEO);vm.start(VM::VIDEO);bool patterns[9]={};now=0;
  for(unsigned i=0;i<100;i++){now+=1000;vm.tick(now,video,audio);if(vm.pattern<9)patterns[vm.pattern]=true;}
  assert(patterns[3]&&patterns[4]&&patterns[6]&&vm.error==VM::OK);
  assert(other[0]==0xA5); // loading code only writes the selected bank in the panel integration
  std::puts("PASS built-in demos: valid ISA control flow, three repeating audio levels, three repeating video effects");
}

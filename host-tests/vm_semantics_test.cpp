#include <cassert>
#include <cstdio>
#include "../firmware/bytereel_2026/vm.h"
static void put(uint8_t*m,uint8_t at,uint8_t op,uint8_t p=0){m[at]=op;m[at+1]=p;}
static void tick(VM&m,uint8_t*v,uint8_t*a,uint32_t t){m.tick(t,v,a);}
int main(){uint8_t v[256]={},a[256]={};VM m;
  m.start(VM::VIDEO);put(v,0,0x40,0xff);put(v,2,0x41,1);tick(m,v,a,0);tick(m,v,a,5);assert(m.a==0&&m.z);
  m.start(VM::VIDEO);put(v,0,0x40,0);put(v,2,0x42,1);tick(m,v,a,0);tick(m,v,a,5);assert(m.a==255&&!m.z);
  m.start(VM::VIDEO);put(v,0,0x40,0x7a);put(v,2,0x45,0x7a);tick(m,v,a,0);tick(m,v,a,5);assert(m.a==0x7a&&m.z);
  m.start(VM::VIDEO);put(v,0,0x40,0xff);put(v,2,0x45,0);tick(m,v,a,0);tick(m,v,a,5);assert(m.a==0xff&&!m.z);
  m.start(VM::VIDEO);put(v,0,0x44,0);tick(m,v,a,0);assert(m.a==0&&m.z);
  m.start(VM::VIDEO);uint32_t r=0x12345678;r^=r<<13;r^=r>>17;r^=r<<5;uint8_t expectedRand=(uint8_t)(((uint64_t)r*256)>>32);put(v,0,0x44,0xff);tick(m,v,a,0);assert(m.a==expectedRand);
  m.start(VM::VIDEO);put(v,0,0x59,0xff);tick(m,v,a,0);assert(m.rng==(0x5f356495u^((uint32_t)256*0x27d4eb2du)));assert(m.videoRng==(0x9e3779b9u^((uint32_t)256*0x045d9f3bu)));
  m.start(VM::VIDEO);put(v,0,0x40,0x77);put(v,2,0x33,0);tick(m,v,a,0);tick(m,v,a,5);assert(m.error==VM::COUNT_ZERO&&m.errorPc==2);
  m.start(VM::VIDEO);put(v,0,0x34,0);tick(m,v,a,0);assert(m.error==VM::LOOP_ZERO);
  m.start(VM::VIDEO);put(v,0,0x33,4);put(v,2,0x40,0);put(v,4,0x41,1);put(v,6,0x34,4);put(v,8,1);for(uint32_t t=0;t<=50;t+=5)tick(m,v,a,t);assert(m.a==4&&m.c==0&&m.pc==10&&!m.running);
  m.start(VM::VIDEO);put(v,0,0x31,3);m.z=false;tick(m,v,a,0);assert(m.error==VM::ODD_TARGET);
  m.start(VM::VIDEO);put(v,0,0x10,0);tick(m,v,a,0);assert(m.error==VM::WRONG_BANK&&m.errorPc==0);
  m.start(VM::AUDIO);put(a,0,0x11,6);tick(m,v,a,0);assert(m.error==VM::BAD_ARGUMENT);
  m.start(VM::VIDEO);put(v,0,0x50,9);tick(m,v,a,0);assert(m.error==VM::BAD_ARGUMENT);
  m.start(VM::VIDEO);put(v,0xfe,1,0xff);m.pc=0xfe;m.fetchNow=true;tick(m,v,a,0);assert(!m.running&&m.error==VM::OK&&m.pc==0x100);

  // All three WAITs add a fixed delay after one speed interval.
  struct WaitCase{VM::Mode mode;uint8_t op,arg;uint32_t delay;};WaitCase waits[]={{VM::VIDEO,3,0x10,160},{VM::AUDIO,0x20,0x10,320},{VM::VIDEO,0x60,0x10,640}};
  for(auto w:waits){uint32_t before=m.executed;uint8_t*mem=w.mode==VM::VIDEO?v:a;m.start(w.mode);put(mem,0,w.op,w.arg);put(mem,2,0x00);put(mem,4,0x00);tick(m,v,a,100);assert(m.deadline==100+5+w.delay);tick(m,v,a,104+w.delay);assert(m.executed==before+1);tick(m,v,a,105+w.delay);assert(m.executed==before+2);tick(m,v,a,110+w.delay);assert(m.executed==before+3);}
  // STEP ignores WAIT, then waits for another NEXT; switching AUTO resumes.
  m.reset(VM::VIDEO);m.step=true;m.start(VM::VIDEO);put(v,0,3,0xff);put(v,2,0x40,9);m.nextStep();tick(m,v,a,0);assert(m.pc==2&&!m.fetchNow);tick(m,v,a,100000);assert(m.pc==2);m.nextStep();tick(m,v,a,100001);assert(m.pc==4&&m.a==9);m.setStep(false,100002);assert(!m.step&&m.deadline==100007);
  m.start(VM::VIDEO);put(v,0,3,0xff);tick(m,v,a,0);m.stop();tick(m,v,a,5000);assert(!m.running&&m.pc==2&&m.pending==VM::NONE);

  // A received 00 is a successful byte; timeout preserves A. STOP cancels an
  // infinite receive and a blocked transmit without a queued delayed retry.
  m.start(VM::AUDIO);m.utime=0;put(a,0,0x92);tick(m,v,a,0);assert(m.pending==VM::RX_WAIT);m.stop();assert(m.pending==VM::NONE&&!m.running);assert(m.uartReceive(0));tick(m,v,a,1);assert(m.pc==2&&m.a==0);
  m.start(VM::AUDIO);m.a=0x6d;m.utime=1;put(a,0,0x92);tick(m,v,a,0);tick(m,v,a,9);assert(m.pending==VM::RX_WAIT);tick(m,v,a,10);assert(m.pending==VM::NONE&&m.a==0x6d&&m.z);
  m.start(VM::AUDIO);m.utime=0;put(a,0,0x90,0x00);tick(m,v,a,0);uint8_t out=0xff;assert(m.uartTransmit(out)&&out==0);assert(m.z==false);
  m.start(VM::AUDIO);m.utime=0;put(a,0,0x90,0xff);tick(m,v,a,0);assert(m.pending==VM::NONE); // Enqueue succeeds on empty TX FIFO.
  m.start(VM::AUDIO);m.utime=0;m.txHead=255;m.txTail=0;put(a,0,0x90,0x42);tick(m,v,a,0);assert(m.pending==VM::TX_WAIT);m.nextStep();assert(m.busySteps==1);m.stop();assert(m.pending==VM::NONE&&m.txHead==m.txTail);assert(!m.uartTransmit(out));
  m.start(VM::AUDIO);m.a=0x55;m.utime=1;put(a,0,0x92);tick(m,v,a,0);tick(m,v,a,10);assert(m.a==0x55&&m.z);
  m.start(VM::AUDIO);m.a=0x33;m.utime=1;m.txHead=255;m.txTail=0;put(a,0,0x90,0xff);tick(m,v,a,0);assert(m.pending==VM::TX_WAIT);tick(m,v,a,9);assert(m.pending==VM::TX_WAIT);tick(m,v,a,10);assert(m.pending==VM::NONE&&m.a==0x33&&m.z&&(m.uartSticky&16));
  m.start(VM::AUDIO);m.a=0x5a;put(a,0,0x93);tick(m,v,a,0);assert(m.a==0x5a&&m.z&&m.uartSticky==0);
  m.start(VM::VIDEO);put(v,0,0x40,0);put(v,2,0x33,2);put(v,4,0x34,4);tick(m,v,a,0);tick(m,v,a,5);m.z=true;tick(m,v,a,10);assert(m.c==1&&m.z&&m.pc==4);
  std::puts("PASS VM semantics: ALU/Z, loop and argument errors, bank/PC edges, fixed waits, STEP, UART success/timeout/STOP");
}

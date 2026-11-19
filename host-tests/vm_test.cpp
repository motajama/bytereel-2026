#include <cassert>
#include <cstdio>
#include <cstring>
#include "../firmware/bytereel_2026/vm.h"
#include "../firmware/bytereel_2026/run_gate.h"
#include "../firmware/bytereel_2026/panel_state.h"
#include "../firmware/bytereel_2026/audio_synth.h"
static void put(uint8_t *m,uint8_t a,uint8_t op,uint8_t p=0){m[a]=op;m[a+1]=p;}
int main(){
 uint8_t v[256]={},a[256]={};VM m;
 m.start(VM::VIDEO);put(v,0,0x40,0xff);m.tick(0,v,a);assert(m.a==255&&!m.z&&m.pc==2&&m.deadline==5);put(v,2,0x41,1);m.tick(4,v,a);assert(m.a==255);m.tick(5,v,a);assert(m.a==0&&m.z);
 m.start(VM::VIDEO);put(v,0xfe,0x00,0xa5);m.pc=0xfe;m.fetchNow=true;m.tick(0,v,a);assert(m.pc==0x100);m.fetchNow=true;m.tick(1,v,a);assert(m.error==VM::PC_RANGE&&m.errorPc==0x100);
 m.start(VM::VIDEO);put(v,0,0x31,3);m.tick(0,v,a);assert(m.error==VM::ODD_TARGET);
 m.start(VM::VIDEO);put(v,0,0x40,0);put(v,2,0x03,2);m.tick(0,v,a);m.tick(5,v,a);assert(m.pc==4&&m.deadline==30);m.tick(29,v,a);assert(m.pc==4);m.tick(30,v,a);assert(m.pc==6);
 m.start(VM::AUDIO);put(a,0,0x92);m.tick(0,v,a);assert(m.pending==VM::RX_WAIT&&m.pc==2);m.uartReceive(0);m.tick(1,v,a);assert(m.pending==VM::NONE&&m.a==0&&!m.z&&m.pc==2);
 m.start(VM::AUDIO);m.utime=1;put(a,0,0x92);m.tick(0,v,a);m.tick(10,v,a);assert(m.pending==VM::NONE&&m.z&&m.a==0&&m.uartSticky==8);
 m.start(VM::AUDIO);put(a,0,0x80,0x50);put(a,2,0x81,0);put(a,4,0x82);m.tick(0,v,a);m.tick(5,v,a);m.tick(10,v,a);assert(m.piezoOn&&m.piezoHz==1000);m.tick(11,v,a);assert(m.piezoOn);m.stop();assert(!m.piezoOn&&m.pending==VM::NONE);
 m.start(VM::VIDEO);m.step=true;put(v,0,3,255);m.tick(0,v,a);assert(m.pc==2&&!m.fetchNow);m.tick(10000,v,a);assert(m.pc==2);m.nextStep();m.tick(10001,v,a);assert(m.pc==4);
 m.step=true;put(v,0,0x40,0x42);m.start(VM::VIDEO);assert(m.running&&m.step&&!m.fetchNow&&m.pc==0);m.tick(0,v,a);assert(m.pc==0&&m.a==0);m.nextStep();m.tick(1,v,a);assert(m.pc==2&&m.a==0x42&&!m.fetchNow);
 m.start(VM::VIDEO);put(v,0,0x40,0xff);put(v,2,0x41,1);m.tick(0,v,a);m.tick(5,v,a);assert(m.a==0&&m.z);
 RunGate gate;gate.boot();assert(!gate.sample(true)&&!gate.running);assert(!gate.sample(true));assert(!gate.sample(false));assert(gate.sample(true)&&gate.running);gate.stoppedByHalt();assert(!gate.sample(true)&&!gate.running);assert(!gate.sample(false));assert(gate.sample(true));gate.reset();assert(!gate.sample(true));gate.modeChanged(false);assert(!gate.sample(true));assert(!gate.sample(false));assert(gate.sample(true));gate.inputFault();assert(!gate.sample(true));
 InputDebouncer db;db.update(0,0xffff,true,true,true,true,true);db.update(5,0xfffe,true,true,true,true,true);assert(db.high(0));db.update(35,0xfffe,true,true,true,true,true);assert(!db.high(0));
 HoldGesture hold;assert(!hold.update(true,2,1,0,2000));assert(!hold.update(true,2,1,1999,2000));assert(hold.update(true,2,1,2000,2000));assert(!hold.update(true,2,1,2100,2000));assert(!hold.update(true,3,1,2200,2000));assert(!hold.active);
 VM av;av.reset(VM::AUDIO);int16_t samples[128];AudioSynth synth;synth.render(av,samples,128);for(int i=0;i<128;i++)assert((uint16_t)samples[i]==0x8000);av.volume=255;av.wave=2;synth.render(av,samples,128);bool differs=false;for(int i=1;i<128;i++)differs|=samples[i]!=samples[0];assert(differs);uint32_t vmRng=av.rng;synth.render(av,samples,128);assert(av.rng==vmRng);
 // Exercise each canonical opcode once with a minimally valid argument and
 // mode. This complements the table-vs-switch source coverage test.
 for(unsigned i=0;i<59;i++){const BrInstructionDef &d=BR_ISA[i];VM probe;VM::Mode mode=!strcmp(d.mode,"AUDIO")?VM::AUDIO:VM::VIDEO;uint8_t arg=1;if(d.opcode==0x00||d.opcode==0x01||d.opcode==0x02||d.opcode==0x10||d.opcode==0x11||d.opcode==0x15||d.opcode==0x30||d.opcode==0x31||d.opcode==0x32||d.opcode==0x34||d.opcode==0x44||d.opcode==0x50||d.opcode==0x70||d.opcode==0x82||d.opcode==0x83||d.opcode==0x92||d.opcode==0x93||d.opcode==0x94||d.opcode==0x96)arg=0;if(d.opcode==0x50)arg=8;probe.start(mode);put(mode==VM::VIDEO?v:a,0,d.opcode,arg);probe.c=2;probe.utime=0;probe.tick(0,v,a);assert(probe.error==VM::OK);if(d.opcode==0x01)assert(!probe.running);}
 std::puts("PASS VM and RunGate: arithmetic, PC edge, branch validation, timing, UART RX, timeout, piezo, STEP, boot interlock, HALT/RESET/fault");
}

#pragma once
#include <stdint.h>
#include <string.h>
#include "isa_table.h"

// Hardware independent ISA 2.0 virtual machine. All time is supplied by caller.
struct VM {
  enum Mode:uint8_t{VIDEO,AUDIO}; enum Error:uint8_t{OK,PC_RANGE,ILLEGAL_OPCODE,WRONG_BANK,BAD_ARGUMENT,ODD_TARGET,COUNT_ZERO,LOOP_ZERO};
  enum Pending:uint8_t{NONE,RX_WAIT,TX_WAIT};
  volatile Mode mode=VIDEO; uint16_t pc=0,intervalMs=5; uint8_t a=0,c=0; bool z=true; volatile bool running=false; bool step=false,fetchNow=false;
  uint32_t rng=0x12345678,deadline=0,waitMs=0,pendingSince=0; uint8_t uartSticky=0,utime=100;
  uint8_t pattern=0,density=0,level=255,scale=1,glitch=0,invert=0,shift=0,feedback=0;
  volatile uint16_t freq=200; volatile uint8_t wave=0,volume=0,attack=0,decay=0,gate=1,pulse=128,crush=8,noise=0,lfoRate=0,lfoDepth=0,filter=255;
  uint16_t piezoHz=1000; uint8_t piezoDuration=10; bool piezoOn=false; uint32_t piezoDeadline=0;
  uint32_t piezoGeneration=0;
  volatile uint32_t synthGeneration=0;
  Error error=OK; uint16_t errorPc=0,requestedStep=0; uint32_t executed=0,uartOverflowCount=0,uartTimeoutCount=0;
  uint32_t fetchSerial=0; uint16_t lastFetchPc=0; uint8_t lastFetchOp=0,lastFetchArg=0;
  Pending pending=NONE; uint8_t pendingOp=0,pendingArg=0,pendingA=0; uint16_t pendingPc=0;
  uint8_t rx[256],tx[256]; uint16_t rxHead=0,rxTail=0,txHead=0,txTail=0;
  static int8_t s8(uint8_t x){return x<128?(int8_t)x:(int8_t)((int16_t)x-256);}
  static uint16_t next(uint16_t p){return (uint16_t)((p+1)&255);}
  uint32_t random32(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
  void reset(Mode m){mode=m;pc=0;a=c=0;z=true;running=false;fetchNow=false;step=false;rng=0x12345678;videoRng=0x9e3779b9u;videoSeed=0;deadline=waitMs=0;error=OK;errorPc=0;lastFetchPc=0;lastFetchOp=lastFetchArg=0;pending=NONE;rxHead=rxTail=txHead=txTail=0;uartSticky=0;utime=100;freq=200;wave=volume=attack=decay=0;gate=1;pulse=128;crush=8;noise=lfoRate=lfoDepth=0;filter=255;piezoHz=1000;piezoDuration=10;piezoOn=false;piezoDeadline=0;++synthGeneration;}
  // RUN resets machine state, but the physical AUTO/STEP switch is panel
  // state, not VM state. In STEP mode the first instruction also waits for
  // NEXT; AUTO starts with an immediate fetch.
  void start(Mode m){bool stepMode=step;reset(m);step=stepMode;running=true;fetchNow=!stepMode;}
  void stop(){running=false;fetchNow=false;waitMs=0;pending=NONE;txTail=txHead;piezoOn=false;piezoDeadline=0;volume=0;}
  void setStep(bool s,uint32_t now){if(step==s)return;step=s;if(!s&&running&&pending==NONE){fetchNow=false;deadline=now+intervalMs;}}
  void nextStep(){if(!running)return;if(pending!=NONE){++busySteps;return;}if(step){fetchNow=true;waitMs=0;}}
  uint32_t busySteps=0;
  bool uartReceive(uint8_t b){uint16_t n=next(rxHead);if(n==rxTail){uartSticky|=4;++uartOverflowCount;return false;}rx[rxHead]=b;rxHead=n;return true;}
  bool uartTransmit(uint8_t &b){if(txTail==txHead)return false;b=tx[txTail];txTail=next(txTail);return true;}
  uint8_t rxCount()const{return (uint8_t)(rxHead-rxTail);}
  void fail(Error e,uint16_t at){error=e;errorPc=at;stop();}
  void completed(uint32_t now,uint32_t extra){++executed;if(!running)return;waitMs=0;if(!step){deadline=now+intervalMs+extra;fetchNow=false;}else fetchNow=false;}
  void tick(uint32_t now,uint8_t *v,uint8_t *aud){
    if(piezoOn&&piezoDuration&&((int32_t)(now-piezoDeadline)>=0))piezoOn=false;
    if(!running)return;
    if(pending!=NONE){pollPending(now);return;}
    if(step&&!fetchNow)return;
    if(!fetchNow&&(int32_t)(now-deadline)<0)return;
    if(pc>0xfe||(pc&1)){fail(PC_RANGE,pc);return;}
    const uint16_t at=pc;uint8_t *mem=mode==VIDEO?v:aud;uint8_t op=mem[pc],p=mem[pc+1];lastFetchPc=at;lastFetchOp=op;lastFetchArg=p;++fetchSerial;pc+=2;uint32_t extra=0;
    #define BAD(e) do{fail(e,at);return;}while(0)
    #define BANK(m) do{if(mode!=(m))BAD(WRONG_BANK);}while(0)
    #define ZFROMA() (z=(a==0))
    const BrInstructionDef *definition=br_instruction(op);if(!definition)BAD(ILLEGAL_OPCODE);if(strcmp(definition->mode,"ANY")&&((mode==VIDEO&&strcmp(definition->mode,"VIDEO"))||(mode==AUDIO&&strcmp(definition->mode,"AUDIO"))))BAD(WRONG_BANK);
    switch(op){
      case 0x00:break;case 0x01:stop();++executed;return;
      case 0x02:freq=200;wave=volume=attack=decay=0;gate=1;pulse=128;crush=8;noise=lfoRate=lfoDepth=0;filter=255;++synthGeneration;break;
      case 0x03:extra=(uint32_t)p*10;break;
      case 0x10:BANK(AUDIO);freq=200+10*p;break;case 0x11:BANK(AUDIO);if(p>5)BAD(BAD_ARGUMENT);wave=p;break;case 0x12:BANK(AUDIO);volume=p;break;case 0x13:BANK(AUDIO);attack=p;break;case 0x14:BANK(AUDIO);decay=p;break;case 0x15:BANK(AUDIO);if(p>1)BAD(BAD_ARGUMENT);if(p&&!gate)envelope=0;gate=p;break;
      case 0x16:BANK(AUDIO);pulse=p;break;case 0x17:BANK(AUDIO);crush=p;break;case 0x18:BANK(AUDIO);noise=p;break;case 0x19:BANK(AUDIO);lfoRate=p;break;case 0x1a:BANK(AUDIO);lfoDepth=p;break;case 0x1b:BANK(AUDIO);filter=p;break;case 0x1e:BANK(AUDIO);freq=200+10*a;break;case 0x1f:BANK(AUDIO);volume=a;break;case 0x20:BANK(AUDIO);extra=(uint32_t)p*20;break;
      case 0x30:case 0x31:case 0x32:case 0x34:case 0x70:
        if(op==0x70)BANK(VIDEO);if(p&1)BAD(ODD_TARGET);
        if(op==0x30||op==0x70||(op==0x31&&z)||(op==0x32&&!z))pc=p;
        else if(op==0x34){if(!c)BAD(LOOP_ZERO);--c;if(c)pc=p;}break;
      case 0x33:if(!p)BAD(COUNT_ZERO);c=p;break;
      case 0x40:a=p;ZFROMA();break;case 0x41:a=(uint8_t)(a+p);ZFROMA();break;case 0x42:a=(uint8_t)(a-p);ZFROMA();break;case 0x43:a^=p;ZFROMA();break;case 0x44:a=(uint8_t)(((uint64_t)random32()*(uint16_t)(p+1))>>32);ZFROMA();break;case 0x45:z=a==p;break;case 0x46:a&=p;ZFROMA();break;case 0x47:a|=p;ZFROMA();break;
      case 0x50:BANK(VIDEO);if(p>8)BAD(BAD_ARGUMENT);pattern=p;break;case 0x51:BANK(VIDEO);motion=s8(p);break;case 0x52:BANK(VIDEO);density=p;break;case 0x53:BANK(VIDEO);level=p;break;case 0x54:BANK(VIDEO);scale=1+p/32;break;case 0x55:BANK(VIDEO);glitch=p;break;case 0x56:BANK(VIDEO);invert=p!=0;break;case 0x57:BANK(VIDEO);shift=s8(p);break;case 0x58:BANK(VIDEO);feedback=p;break;case 0x59:BANK(VIDEO);videoSeed=p;rng=0x5f356495u^((uint32_t)(p+1)*0x27d4eb2du);videoRng=0x9e3779b9u^((uint32_t)(p+1)*0x045d9f3bu);break;case 0x5e:BANK(VIDEO);density=a;break;case 0x5f:BANK(VIDEO);glitch=a;break;case 0x60:BANK(VIDEO);extra=(uint32_t)p*40;break;
      case 0x80:piezoHz=200+10*p;break;case 0x81:piezoDuration=p;break;case 0x82:piezoOn=true;piezoDeadline=piezoDuration?now+(uint32_t)piezoDuration*10:0;++piezoGeneration;break;case 0x83:piezoOn=false;break;case 0x84:piezoHz=200+10*a;break;
      case 0x90:case 0x91:pendingA=op==0x90?p:a;pendingOp=op;pendingArg=p;pendingPc=at;pendingSince=now;pending=TX_WAIT;pollPending(now);return;
      case 0x92:pending=RX_WAIT;pendingOp=op;pendingPc=at;pendingSince=now;pollPending(now);return;
      case 0x93:if(rxTail!=rxHead){a=rx[rxTail];rxTail=next(rxTail);z=false;}else z=true;break;
      case 0x94:a=rxCount();ZFROMA();break;case 0x95:utime=p;break;case 0x96:a=(uint8_t)((rxTail!=rxHead?1:0)|((next(txHead)!=txTail)?2:0)|uartSticky);ZFROMA();break;case 0x97:uartSticky&=(uint8_t)~(p?p:0x3c);break;
      default:BAD(ILLEGAL_OPCODE);
    }
    completed(now,extra);
    #undef BANK
    #undef BAD
  }
  bool envelopeValid=false; float envelope=1.0f; int16_t motion=0; uint8_t videoSeed=0; uint32_t videoRng=0x9e3779b9u;
  void pollPending(uint32_t now){
    if(pending==RX_WAIT&&rxTail!=rxHead){a=rx[rxTail];rxTail=next(rxTail);z=false;pending=NONE;completed(now,0);return;}
    if(pending==TX_WAIT&&next(txHead)!=txTail){tx[txHead]=pendingA;txHead=next(txHead);z=false;pending=NONE;completed(now,0);return;}
    if(utime&&((uint32_t)(now-pendingSince)>=(uint32_t)utime*10)){if(pending==RX_WAIT)uartSticky|=8;else uartSticky|=16;++uartTimeoutCount;z=true;pending=NONE;completed(now,0);}
  }
};

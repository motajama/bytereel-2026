#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

struct BrFetchEvent { uint16_t address; uint8_t value; uint8_t kind; };
struct BrFetchFifo {
  static const uint8_t CAPACITY=48;
  BrFetchEvent events[CAPACITY]{}; uint8_t head=0,count=0; uint32_t dropped=0;
  void push(uint16_t address,uint8_t value,uint8_t kind){
    uint8_t at;
    if(count==CAPACITY){at=head;head=(uint8_t)((head+1)%CAPACITY);--count;++dropped;}
    at=(uint8_t)((head+count)%CAPACITY);events[at]={address,value,kind};++count;
  }
  bool pop(BrFetchEvent &event){if(!count)return false;event=events[head];head=(uint8_t)((head+1)%CAPACITY);--count;return true;}
  void clear(){head=count=0;}
};

// Fixed 8 KiB application log: 64 entries, exactly 128 bytes each.
struct BrLogEntry { uint32_t sequence=0,atMs=0; char type[24]{}; char text[96]{}; };
struct BrLogRing {
  static const uint8_t CAPACITY=64;
  BrLogEntry entries[CAPACITY]{}; uint32_t nextSequence=1;
  uint32_t append(uint32_t atMs,const char *message){
    return append(atMs,"INFO",message);
  }
  uint32_t append(uint32_t atMs,const char *type,const char *message){
    const uint32_t seq=nextSequence++;
    BrLogEntry &e=entries[(seq-1)%CAPACITY];e.sequence=seq;e.atMs=atMs;
    if(!type)type="INFO";strncpy(e.type,type,sizeof(e.type)-1);e.type[sizeof(e.type)-1]=0;
    if(!message)message="";strncpy(e.text,message,sizeof(e.text)-1);e.text[sizeof(e.text)-1]=0;
    return seq;
  }
  uint32_t oldest()const{return nextSequence>CAPACITY?nextSequence-CAPACITY:1;}
  bool lost(uint32_t after)const{return after+1<oldest();}
  const BrLogEntry *find(uint32_t sequence)const{
    if(sequence<oldest()||sequence>=nextSequence)return nullptr;
    const BrLogEntry &e=entries[(sequence-1)%CAPACITY];return e.sequence==sequence?&e:nullptr;
  }
};
struct BrLogCursor {
  uint32_t sequence=0;bool lost=false;
  void begin(uint32_t requestedBoot,uint32_t currentBoot,uint32_t after,uint32_t oldest){lost=requestedBoot!=currentBoot;sequence=lost?0:after;if(sequence+1<oldest){lost=true;sequence=oldest-1;}}
  void advance(uint32_t emitted){sequence=emitted;}
};

struct BrRuntimeCounters {
  uint32_t i2cErrors=0,uartRx=0,uartTx=0,uartRxOverflow=0,uartTimeout=0;
  uint32_t audioWriteErrors=0,audioUnderruns=0,videoDroppedFrames=0;
  uint32_t executedAtSample=0; uint16_t requestedIps=1,measuredIps=0;
  uint32_t sampleAt=0;
  void sample(uint32_t now,uint32_t executed){if((uint32_t)(now-sampleAt)>=1000){uint32_t dt=now-sampleAt;if(sampleAt)measuredIps=(uint16_t)(((uint64_t)(executed-executedAtSample)*1000)/dt);sampleAt=now;executedAtSample=executed;}}
};

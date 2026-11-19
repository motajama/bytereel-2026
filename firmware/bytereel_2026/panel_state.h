#pragma once
#include <stdint.h>

struct InputDebouncer {
  struct Key { bool raw=true,stable=true,initialized=false;uint32_t changed=0; } keys[21];
  uint16_t buttons=0xffff; uint8_t modes=0x1f;
  uint32_t pressed=0,released=0;
  uint32_t reacquireAt=0; bool reacquireStarted=false,ready=false;
  void invalidate(){
    ready=false;reacquireStarted=false;pressed=released=0;buttons=0xffff;modes=0x1f;
    for(unsigned i=0;i<21;i++){keys[i].initialized=false;keys[i].raw=keys[i].stable=true;keys[i].changed=0;}
  }
  bool update(uint32_t now,uint16_t expander,bool runHigh,bool videoHigh,bool liveHigh,bool autoHigh,bool editHigh){
    const bool levels[21]={
      (expander&1)!=0,(expander&2)!=0,(expander&4)!=0,(expander&8)!=0,(expander&16)!=0,(expander&32)!=0,(expander&64)!=0,(expander&128)!=0,
      (expander&256)!=0,(expander&512)!=0,(expander&1024)!=0,(expander&2048)!=0,(expander&4096)!=0,(expander&8192)!=0,
      (expander&16384)!=0,(expander&32768)!=0,
      runHigh,videoHigh,liveHigh,autoHigh,editHigh
    };
    bool changed=false;pressed=released=0;
    for(unsigned i=0;i<21;i++){
      Key &k=keys[i];if(!k.initialized){k.raw=levels[i];k.stable=levels[i];k.changed=now;k.initialized=true;continue;}
      if(levels[i]!=k.raw){k.raw=levels[i];k.changed=now;}
      if(k.stable!=k.raw&&(uint32_t)(now-k.changed)>=30){bool old=k.stable;k.stable=k.raw;changed=true;if(old&&!k.stable)pressed|=(1u<<i);if(!old&&k.stable)released|=(1u<<i);}
    }
    buttons=0;for(unsigned i=0;i<16;i++)if(keys[i].stable)buttons|=(uint16_t)(1u<<i);
    modes=0;for(unsigned i=0;i<5;i++)if(keys[i+16].stable)modes|=(uint8_t)(1u<<i);
    if(!ready){
      if(!reacquireStarted){reacquireStarted=true;reacquireAt=now;}
      bool settled=(uint32_t)(now-reacquireAt)>=30;
      for(unsigned i=0;i<21;i++)if(!keys[i].initialized||keys[i].raw!=keys[i].stable||(uint32_t)(now-keys[i].changed)<30)settled=false;
      if(settled)ready=true;
      // Edge events during initial acquisition or recovery are not user
      // gestures. Require a fully settled sample before the panel acts.
      pressed=released=0;
    }
    return changed;
  }
  bool high(unsigned index)const{return index<21?keys[index].stable:true;}
  bool activeLow(unsigned index)const{return !high(index);}
};

struct HoldGesture {
  bool active=false,done=false; uint32_t started=0; uint8_t capturedSlot=0,capturedMode=0;
  bool update(bool held,uint8_t slot,uint8_t mode,uint32_t now,uint32_t duration){
    if(!held){cancel();return false;}
    if(done&&!active)return false;
    if(!active){active=true;done=false;started=now;capturedSlot=slot;capturedMode=mode;}
    if(slot!=capturedSlot||mode!=capturedMode){active=false;done=true;return false;}
    if(!done&&(uint32_t)(now-started)>=duration){done=true;return true;}
    return false;
  }
  void cancel(){active=false;done=false;}
};

#pragma once
#include <stdint.h>
#include <string.h>

enum PanelSlotResult:uint8_t { PANEL_SLOT_OK,PANEL_SLOT_RUN,PANEL_SLOT_FILESYSTEM,PANEL_SLOT_EMPTY,PANEL_SLOT_CORRUPT,PANEL_SLOT_MEMORY,PANEL_SLOT_WRITE_FAILED };

// Logical LED addresses are independent: upper row reverses A0..A7 to match
// front-panel A7..A0 orientation; lower DATA row preserves the prior mapping.
static inline uint8_t br_reverse_bits(uint8_t v) {
  v=(uint8_t)(((v&0x55u)<<1)|((v>>1)&0x55u));
  v=(uint8_t)(((v&0x33u)<<2)|((v>>2)&0x33u));
  return (uint8_t)((v<<4)|(v>>4));
}
static inline uint16_t br_panel_map_word(uint16_t logical) {
  const uint8_t address=(uint8_t)(logical>>8),data=(uint8_t)logical;
  return (uint16_t)(((uint16_t)br_reverse_bits(address)<<8)|data);
}
static inline uint16_t br_panel_led_word(uint8_t address,uint8_t data) {
  return br_panel_map_word((uint16_t)(((uint16_t)address<<8)|data));
}
static inline uint8_t br_edit_display_value(bool preview,uint8_t switches,uint8_t stored){return preview?switches:stored;}
static inline void br_edit_store(uint8_t *bank,uint8_t &address,uint8_t value,bool advance){if(!bank)return;bank[address]=value;if(advance)++address;}
static inline bool br_publish_banks(bool verified,const uint8_t *video,const uint8_t *audio,uint8_t *videoRam,uint8_t *audioRam){if(!verified||!video||!audio||!videoRam||!audioRam)return false;memcpy(videoRam,video,256);memcpy(audioRam,audio,256);return true;}

struct BrEditPreview {
  bool active=false;
  uint8_t observed=0;
  bool paddleChanged(uint8_t value) {
    bool changed=value!=observed;observed=value;
    if(!active&&changed)active=true;
    return changed;
  }
  void examPress(uint8_t value,uint8_t &address) {
    if(active){active=false;observed=value;return;}
    address=value;observed=value;
  }
  void reset(){active=false;observed=0;}
};

static inline bool br_one_hot_slot(uint8_t switches,uint8_t &slot) {
  if(!switches || (switches&(uint8_t)(switches-1)))return false;
  for(slot=0;slot<8;slot++)if(switches==(uint8_t)(1u<<slot))return true;
  return false;
}

struct BrErrorPulse {
  bool active=false;
  uint8_t step=0;
  uint32_t changedAt=0;
  void trigger(uint32_t now){active=true;step=0;changedAt=now;}
  bool update(uint32_t now,bool ledHealthy,bool inputHealthy){
    if(!ledHealthy||!inputHealthy){active=false;return false;}
    if(!active)return false;
    const uint32_t phases=(uint32_t)(now-changedAt)/100;
    if(phases){changedAt+=phases*100;step=(uint8_t)(step+(phases>6?6:phases));if(step>=6){active=false;return false;}}
    return active;
  }
  bool allOn()const{return (step&1)==0;}
};
static inline uint16_t br_error_pulse_word(const BrErrorPulse &pulse,uint16_t normal){return pulse.active?(pulse.allOn()?0xffff:0x0000):normal;}

struct BrSlowBlink {
  bool active=false,on=true;uint32_t changedAt=0;
  void enter(uint32_t now){active=true;on=true;changedAt=now;}
  void leave(){active=false;on=true;}
  bool update(uint32_t now){
    if(!active)return true;
    const uint32_t phases=(uint32_t)(now-changedAt)/1000;
    if(phases){if(phases&1u)on=!on;changedAt+=phases*1000;}
    return on;
  }
};

// Observe debounced EDIT state. A true return means the network lifecycle
// should be applied exactly once, including the first stable panel sample.
struct BrUiModeGate {
  bool known=false,memory=false;
  bool observe(bool edit){const bool wantedMemory=!edit;if(!known||memory!=wantedMemory){known=true;memory=wantedMemory;return true;}return false;}
};

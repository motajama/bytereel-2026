#include <cassert>
#include <cstdio>
#include "../firmware/bytereel_2026/panel_display.h"
#include "../firmware/bytereel_2026/panel_state.h"
#include "../firmware/bytereel_2026/monitor_state.h"
#include "../firmware/bytereel_2026/storage_boot_guard.h"

int main(){
  const uint8_t cases[]={0x01,0x80,0x55,0xaa};
  const uint8_t reversed[]={0x80,0x01,0xaa,0x55};
  for(unsigned i=0;i<4;i++){
    assert(br_panel_led_word(cases[i],cases[i])==((uint16_t)reversed[i]<<8|cases[i]));
  }
  for(uint8_t i=0;i<8;i++){uint8_t slot=99;assert(br_one_hot_slot((uint8_t)(1u<<i),slot)&&slot==i);}
  uint8_t slot=0;assert(!br_one_hot_slot(0,slot));assert(!br_one_hot_slot(3,slot));assert(!br_one_hot_slot(0xff,slot));
  BrEditPreview p;uint8_t address=7;
  p.observed=0x01;p.paddleChanged(0x01);assert(!p.active);
  uint8_t bank[256]={},previewShown=br_edit_display_value(true,0x02,bank[address]);p.paddleChanged(0x02);assert(p.active&&previewShown==0x02&&bank[address]==0);
  assert(address==7); // paddle preview is read-only
  p.examPress(0x02,address);assert(!p.active&&address==7);
  p.paddleChanged(0x02);assert(!p.active); // held value cannot re-enter
  p.paddleChanged(0x04);assert(p.active);
  p.examPress(0x04,address);assert(!p.active&&address==7);
  p.examPress(0x2a,address);assert(address==0x2a); // separate press selects address
  address=7;br_edit_store(bank,address,0x02,false);assert(address==7&&bank[7]==0x02);br_edit_store(bank,address,0x80,true);assert(address==8&&bank[7]==0x80);
  uint8_t vr[256],ar[256],stagedV[256],stagedA[256];memset(vr,0xa5,sizeof(vr));memset(ar,0x5a,sizeof(ar));memset(stagedV,0x11,sizeof(stagedV));memset(stagedA,0x22,sizeof(stagedA));assert(!br_publish_banks(false,stagedV,stagedA,vr,ar));assert(vr[0]==0xa5&&ar[0]==0x5a);assert(br_publish_banks(true,stagedV,stagedA,vr,ar));assert(vr[0]==0x11&&ar[0]==0x22);
  HoldGesture h;assert(!h.update(true,1,0,0,100));assert(!h.update(true,2,0,30,100));assert(!h.update(true,2,0,200,100));assert(!h.update(false,2,0,210,100));assert(!h.update(true,2,0,220,100));
  assert(br_storage_recovery_checked(false,true,BR_PARTITION_ERASED)==BR_STORAGE_RECOVERY_FORMAT_BLANK);
  assert(br_storage_recovery_checked(false,true,BR_PARTITION_HAS_DATA)==BR_STORAGE_RECOVERY_REFUSE_DATA);
  assert(br_storage_recovery_checked(false,true,BR_PARTITION_READ_FAILED)==BR_STORAGE_RECOVERY_REFUSE_DATA);
  assert(br_storage_recovery_checked(false,false,BR_PARTITION_ERASED)==BR_STORAGE_RECOVERY_REQUIRE_STOP);
  assert(br_storage_recovery_checked(true,false,BR_PARTITION_HAS_DATA)==BR_STORAGE_RECOVERY_MOUNTED);
  BrErrorPulse pulse;pulse.trigger(0);uint16_t intended=br_panel_led_word(0x42,0x81);bool active[7];uint16_t shown=intended;for(unsigned i=0;i<7;i++){active[i]=pulse.update(i*100,true,true);shown=br_error_pulse_word(pulse,intended);}assert(active[0]&&active[1]&&active[2]&&active[3]&&active[4]&&active[5]&&!active[6]);assert(shown==intended);
  BrSlowBlink blink;blink.enter(50);assert(blink.update(50));assert(blink.update(1049));assert(!blink.update(1050));assert(blink.update(2050));blink.enter(3000);assert(blink.update(3000));blink.leave();assert(blink.update(9000));
  BrUiModeGate gate;int wifiStarts=0,wifiStops=0;uint8_t ramSentinel=0x5a;BrLogRing logs;logs.append(1,"RAM","preserve me");assert(gate.observe(true)&&!gate.memory);for(unsigned i=0;i<100;i++){assert(gate.observe(false)&&gate.memory);++wifiStarts;assert(!gate.observe(false));assert(gate.observe(true)&&!gate.memory);++wifiStops;assert(!gate.observe(true));}assert(wifiStarts==100&&wifiStops==100&&ramSentinel==0x5a);assert(logs.nextSequence==2&&!strcmp(logs.find(1)->text,"preserve me"));
  std::puts("PASS panel display: row mapping, slot selector, EDIT preview, memory-safe mode gate, 1s blink and 100ms error flashes");
}

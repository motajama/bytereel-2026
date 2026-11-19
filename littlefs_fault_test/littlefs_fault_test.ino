#include <Arduino.h>
#include <LittleFS.h>
#include "slot_store.h"
#include "littlefs_slot_adapter.h"

static const char *TEST_DIR="/br26-fault-test";
static const size_t CAP=BR_SLOT_MAX_PAYLOAD+32;
static uint8_t scratchA[CAP],scratchB[CAP];
static const uint8_t OLD_PAIR[]="{\"v\":\"old-VIDEO\",\"a\":\"old-AUDIO\"}";
static const uint8_t NEW_PAIR[]="{\"v\":\"new-VIDEO\",\"a\":\"new-AUDIO\"}";
struct TruncatingStore {
  BrLittleFsStore base; size_t cut; bool failAfterComplete;
  TruncatingStore(size_t n,bool after=false):base(LittleFS,TEST_DIR,true),cut(n),failAfterComplete(after){}
  int read(uint8_t id,char side,uint8_t*out,size_t n){return base.read(id,side,out,n);}
  bool write(uint8_t id,char side,const uint8_t*data,size_t n){
    File f=LittleFS.open(base.path(id,side),"w");if(!f)return false;
    size_t count=cut<n?cut:n;size_t wrote=f.write(data,count);f.flush();f.close();
    return wrote==n&&!failAfterComplete;
  }
};
static bool requireOldPair(){BrLittleFsStore reopened(LittleFS,TEST_DIR,true);int an=reopened.read(0,'a',scratchA,CAP),bn=reopened.read(0,'b',scratchB,CAP);BrSlotView selected;BrSlotState s=br_select_slot(scratchA,an<0?0:an,scratchB,bn<0?0:bn,selected);return (s==BR_SLOT_VALID||s==BR_SLOT_DEGRADED)&&selected.generation==1&&selected.length==sizeof(OLD_PAIR)&&!memcmp(selected.payload,OLD_PAIR,sizeof(OLD_PAIR));}
static void finish(bool ok,const char*message){Serial.printf("%s %s\n",ok?"PASS":"FAIL",message);if(LittleFS.exists(String(TEST_DIR)+"/00.a.brp"))LittleFS.remove(String(TEST_DIR)+"/00.a.brp");if(LittleFS.exists(String(TEST_DIR)+"/00.b.brp"))LittleFS.remove(String(TEST_DIR)+"/00.b.brp");LittleFS.rmdir(TEST_DIR);LittleFS.end();}
void setup(){Serial.begin(115200);delay(200);if(ESP.getFlashChipSize()<4*1024*1024){Serial.println("FAIL flash below 4 MiB; no FS access");return;}if(!LittleFS.begin(false,"/littlefs",4,"spiffs")){Serial.println("FAIL LittleFS mount; begin(false), no format attempted");return;}if(LittleFS.exists(TEST_DIR)){Serial.println("REFUSE existing /br26-fault-test; no files changed");LittleFS.end();return;}if(!LittleFS.mkdir(TEST_DIR)){Serial.println("FAIL mkdir; no format attempted");LittleFS.end();return;}
  BrLittleFsStore store(LittleFS,TEST_DIR,true);uint32_t revision=0;if(br_commit_slot(store,0,OLD_PAIR,sizeof(OLD_PAIR),0,revision,scratchA,scratchB,CAP)!=BR_COMMIT_OK||revision!=1){finish(false,"initial LittleFS pair commit");return;}
  const size_t encodedLength=32+sizeof(NEW_PAIR);
  for(size_t cut=0;cut<encodedLength;cut++){TruncatingStore fault(cut);uint32_t next=0;(void)br_commit_slot(fault,0,NEW_PAIR,sizeof(NEW_PAIR),1,next,scratchA,scratchB,CAP);LittleFS.end();if(!LittleFS.begin(false,"/littlefs",4,"spiffs")){Serial.println("FAIL remount after truncation; no format attempted");return;}if(!requireOldPair()){finish(false,"reopen after truncated candidate lost previous complete pair");return;}}
  if(!requireOldPair()){finish(false,"old pair before successful commit");return;}
  TruncatingStore lostAck(encodedLength,true);uint32_t ignored=0;(void)br_commit_slot(lostAck,0,NEW_PAIR,sizeof(NEW_PAIR),1,ignored,scratchA,scratchB,CAP);LittleFS.end();if(!LittleFS.begin(false,"/littlefs",4,"spiffs")){Serial.println("FAIL remount after full write; no format attempted");return;}BrLittleFsStore finalStore(LittleFS,TEST_DIR,true);int an=finalStore.read(0,'a',scratchA,CAP),bn=finalStore.read(0,'b',scratchB,CAP);BrSlotView selected;BrSlotState state=br_select_slot(scratchA,an<0?0:an,scratchB,bn<0?0:bn,selected);bool ok=(state==BR_SLOT_VALID||state==BR_SLOT_DEGRADED)&&selected.generation==2&&selected.length==sizeof(NEW_PAIR)&&!memcmp(selected.payload,NEW_PAIR,sizeof(NEW_PAIR));finish(ok,"LittleFS truncation sweep, remount selection and full-write lost-ack simulation; ENOSPC is simulated, not physically induced");}
void loop(){}

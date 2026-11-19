#include <cassert>
#include <cstdio>
#include <vector>
#include <cstring>
#include "../firmware/bytereel_2026/slot_store.h"

struct FaultStore {
  std::vector<uint8_t> files[8][2]; int failAfter=-1;bool failAfterWrite=false,readError=false;char failRemove='\0';
  int read(uint8_t id,char side,uint8_t*out,size_t cap){if(readError)return -2;auto &f=files[id][side=='a'?0:1];if(f.empty())return -1;if(f.size()>cap)return -2;memcpy(out,f.data(),f.size());return (int)f.size();}
  bool write(uint8_t id,char side,const uint8_t*data,size_t n){auto &f=files[id][side=='a'?0:1];size_t take=failAfter<0?n:(size_t)failAfter;if(take>n)take=n;f.assign(data,data+take);return take==n&&!failAfterWrite;}
  bool remove(uint8_t id,char side){if(side==failRemove)return false;files[id][side=='a'?0:1].clear();return true;}
};
static BrSlotState state(FaultStore&s,uint8_t*a,uint8_t*b,BrSlotView&v){int an=s.read(0,'a',a,BR_SLOT_MAX_PAYLOAD+32),bn=s.read(0,'b',b,BR_SLOT_MAX_PAYLOAD+32);return br_select_slot(a,an<0?0:an,b,bn<0?0:bn,v);}
int main(){
  const size_t cap=BR_SLOT_MAX_PAYLOAD+32;std::vector<uint8_t>a(cap),b(cap);const uint8_t oldPair[]="old-video-and-audio-pair",newPair[]="new-video-and-audio-pair";uint32_t revision=0;FaultStore store;BrCommitPhase phase=BR_COMMIT_PHASE_NONE;
  assert(br_commit_slot(store,0,oldPair,sizeof(oldPair),0,revision,a.data(),b.data(),cap,&phase)==BR_COMMIT_OK&&revision==1&&phase==BR_COMMIT_PHASE_VERIFY_CRC);
  const uint8_t repeatedPair[]="repeated-panel-save-pair";FaultStore repeated=store;assert(br_commit_slot(repeated,0,repeatedPair,sizeof(repeatedPair),1,revision,a.data(),b.data(),cap,&phase)==BR_COMMIT_OK&&revision==2&&phase==BR_COMMIT_PHASE_VERIFY_CRC);
  {BrSlotView loaded;assert(state(repeated,a.data(),b.data(),loaded)==BR_SLOT_VALID&&loaded.generation==2&&loaded.length==sizeof(repeatedPair)&&!memcmp(loaded.payload,repeatedPair,sizeof(repeatedPair)));}
  assert(br_commit_slot(store,0,newPair,sizeof(newPair),0,revision,a.data(),b.data(),cap,&phase)==BR_COMMIT_CONFLICT&&phase==BR_COMMIT_PHASE_REVISION);
  revision=1;FaultStore empty;
  assert(br_commit_slot(empty,1,oldPair,sizeof(oldPair),0,revision,a.data(),b.data(),cap,&phase)==BR_COMMIT_OK&&revision==1&&phase==BR_COMMIT_PHASE_VERIFY_CRC);
  for(int cut=0;cut<(int)(32+sizeof(newPair));cut++){
    FaultStore trial=store;trial.failAfter=cut;uint32_t out=0;
    assert(br_commit_slot(trial,0,newPair,sizeof(newPair),1,out,a.data(),b.data(),cap)==BR_COMMIT_IO);
    // Reopen means no cached state: select from the two persistent candidates.
    BrSlotView selected;BrSlotState recovered=state(trial,a.data(),b.data(),selected);assert(recovered==BR_SLOT_VALID||recovered==BR_SLOT_DEGRADED);assert(selected.generation==1&&selected.length==sizeof(oldPair)&&!memcmp(selected.payload,oldPair,sizeof(oldPair)));
  }
  {FaultStore trial=store;trial.failAfter=(int)(32+sizeof(newPair));trial.failAfterWrite=true;uint32_t out=0;assert(br_commit_slot(trial,0,newPair,sizeof(newPair),1,out,a.data(),b.data(),cap)==BR_COMMIT_IO);BrSlotView selected;assert(state(trial,a.data(),b.data(),selected)==BR_SLOT_VALID);assert(selected.generation==2&&!memcmp(selected.payload,newPair,sizeof(newPair)));}
  {FaultStore trial=store;trial.readError=true;uint32_t out=0;assert(br_commit_slot(trial,0,newPair,sizeof(newPair),1,out,a.data(),b.data(),cap)==BR_COMMIT_IO);}
  {uint32_t out=0;assert(br_commit_slot(store,0,newPair,sizeof(newPair),0,out,a.data(),b.data(),cap)==BR_COMMIT_CONFLICT);}
  {FaultStore trial=store;trial.failRemove='a';assert(br_delete_slot(trial,0,1,a.data(),b.data(),cap)==BR_DELETE_IO);BrSlotView selected;assert(state(trial,a.data(),b.data(),selected)==BR_SLOT_VALID&&selected.generation==1&&!memcmp(selected.payload,oldPair,sizeof(oldPair)));}
  {FaultStore trial=store;uint32_t r=0;assert(br_commit_slot(trial,0,newPair,sizeof(newPair),1,r,a.data(),b.data(),cap)==BR_COMMIT_OK&&r==2);trial.failRemove='b';assert(br_delete_slot(trial,0,2,a.data(),b.data(),cap)==BR_DELETE_IO);BrSlotView selected;assert(state(trial,a.data(),b.data(),selected)==BR_SLOT_VALID&&selected.generation==2&&!memcmp(selected.payload,newPair,sizeof(newPair)));}
  {FaultStore trial=store;assert(br_delete_slot(trial,0,2,a.data(),b.data(),cap)==BR_DELETE_CONFLICT);BrSlotView selected;assert(state(trial,a.data(),b.data(),selected)==BR_SLOT_VALID&&selected.generation==1);assert(br_delete_slot(trial,0,1,a.data(),b.data(),cap)==BR_DELETE_OK);assert(state(trial,a.data(),b.data(),selected)==BR_SLOT_EMPTY);}
  std::puts("PASS slot persistence algorithms: commit fault sweep, revision protection, delete failure preserves a valid candidate");
}

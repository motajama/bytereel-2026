#include <cassert>
#include <cstdio>
#include <cstring>
#include "../firmware/bytereel_2026/slot_envelope.h"
int main(){const uint8_t p[]="{\"schema\":1,\"isa\":\"2.0\",\"slot\":0}";uint8_t old[256],fresh[256],bad[256];size_t on,fn,bn;assert(br_encode_slot(p,sizeof(p)-1,1,old,sizeof(old),on));assert(br_encode_slot(p,sizeof(p)-1,2,fresh,sizeof(fresh),fn));BrSlotView v;assert(br_decode_slot(old,on,v)&&v.generation==1&&v.length==sizeof(p)-1);assert(br_select_slot(old,on,fresh,fn,v)==BR_SLOT_VALID&&v.generation==2);assert(br_select_slot(old,on,nullptr,0,v)==BR_SLOT_VALID&&v.generation==1);assert(br_select_slot(nullptr,0,nullptr,0,v)==BR_SLOT_EMPTY);
 for(size_t n=0;n<fn;n++){BrSlotView t;assert(!br_decode_slot(fresh,n,t));BrSlotState s=br_select_slot(old,on,fresh,n,t);assert((n==0?s==BR_SLOT_VALID:s==BR_SLOT_DEGRADED)&&t.generation==1);}
 memcpy(bad,fresh,fn);bad[12]^=1;assert(!br_decode_slot(bad,fn,v));memcpy(bad,fresh,fn);bad[fn-1]^=1;assert(!br_decode_slot(bad,fn,v));memcpy(bad,fresh,fn);bad[fn]=0;assert(!br_decode_slot(bad,fn+1,v));
 const uint8_t q[]="different";assert(br_encode_slot(q,sizeof(q)-1,2,bad,sizeof(bad),bn));assert(br_select_slot(fresh,fn,bad,bn,v)==BR_SLOT_CORRUPT);
 uint32_t nextRev=0;assert(br_next_revision(BR_SLOT_EMPTY,0,0,nextRev)==BR_REVISION_OK&&nextRev==1);assert(br_next_revision(BR_SLOT_VALID,3,3,nextRev)==BR_REVISION_OK&&nextRev==4);assert(br_next_revision(BR_SLOT_VALID,4,3,nextRev)==BR_REVISION_CONFLICT);assert(br_next_revision(BR_SLOT_VALID,0xffffffffu,0xffffffffu,nextRev)==BR_REVISION_EXHAUSTED);
 std::puts("PASS slot envelope: CRC/header/payload/truncation/trailing, generation selection, duplicate conflict");}

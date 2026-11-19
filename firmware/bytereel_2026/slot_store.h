#pragma once
#include "slot_envelope.h"

enum BrCommitResult:uint8_t { BR_COMMIT_OK,BR_COMMIT_CONFLICT,BR_COMMIT_CORRUPT,BR_COMMIT_EXHAUSTED,BR_COMMIT_IO,BR_COMMIT_VERIFY };
enum BrCommitPhase:uint8_t { BR_COMMIT_PHASE_NONE,BR_COMMIT_PHASE_READ_CANDIDATES,BR_COMMIT_PHASE_REVISION,BR_COMMIT_PHASE_ENCODE,BR_COMMIT_PHASE_WRITE_CANDIDATE,BR_COMMIT_PHASE_VERIFY_READ,BR_COMMIT_PHASE_VERIFY_CRC };
static inline const char *br_commit_phase_name(BrCommitPhase p){switch(p){case BR_COMMIT_PHASE_READ_CANDIDATES:return "READ_CANDIDATES";case BR_COMMIT_PHASE_REVISION:return "REVISION";case BR_COMMIT_PHASE_ENCODE:return "ENCODE";case BR_COMMIT_PHASE_WRITE_CANDIDATE:return "WRITE_CANDIDATE";case BR_COMMIT_PHASE_VERIFY_READ:return "VERIFY_READ";case BR_COMMIT_PHASE_VERIFY_CRC:return "VERIFY_CRC";default:return "NONE";}}
static inline const char *br_commit_result_name(BrCommitResult r){switch(r){case BR_COMMIT_OK:return "OK";case BR_COMMIT_CONFLICT:return "REVISION_CONFLICT";case BR_COMMIT_CORRUPT:return "SLOT_CORRUPT";case BR_COMMIT_EXHAUSTED:return "REVISION_EXHAUSTED";case BR_COMMIT_VERIFY:return "VERIFY_FAILED";default:return "IO_FAILED";}}
enum BrDeleteResult:uint8_t { BR_DELETE_OK,BR_DELETE_CONFLICT,BR_DELETE_CORRUPT,BR_DELETE_IO };

// Adapter contract: read() returns -1 for missing, -2 for I/O error, otherwise
// byte count. write() replaces only the requested candidate (never both).
template<class Store>
static BrCommitResult br_commit_slot(Store &store,uint8_t id,const uint8_t *payload,size_t length,
  uint32_t expected,uint32_t &revision,uint8_t *a,uint8_t *b,size_t capacity,BrCommitPhase *phase=nullptr){
  if(phase)*phase=BR_COMMIT_PHASE_READ_CANDIDATES;
  if(id>7||!payload||length>BR_SLOT_MAX_PAYLOAD||!a||!b)return BR_COMMIT_IO;
  int ar=store.read(id,'a',a,capacity),br=store.read(id,'b',b,capacity);
  if(ar==-2||br==-2||ar>(int)capacity||br>(int)capacity)return BR_COMMIT_IO;
  size_t an=ar<0?0:(size_t)ar,bn=br<0?0:(size_t)br;
  BrSlotView av,bv,current;bool oka=br_decode_slot(a,an,av),okb=br_decode_slot(b,bn,bv);
  BrSlotState state=br_select_slot(a,an,b,bn,current);
  uint32_t actual=state==BR_SLOT_EMPTY?0:current.generation,next=0;
  if(phase)*phase=BR_COMMIT_PHASE_REVISION;
  BrRevisionStatus rs=br_next_revision(state,actual,expected,next);
  if(rs==BR_REVISION_CONFLICT)return BR_COMMIT_CONFLICT;
  if(rs==BR_REVISION_CORRUPT)return BR_COMMIT_CORRUPT;
  if(rs==BR_REVISION_EXHAUSTED)return BR_COMMIT_EXHAUSTED;
  char target=(!oka||(okb&&bv.generation<=av.generation))?'a':'b';
  uint8_t *encoded=target=='a'?a:b;size_t encodedLength=0;
  if(phase)*phase=BR_COMMIT_PHASE_ENCODE;
  if(!br_encode_slot(payload,(uint32_t)length,next,encoded,capacity,encodedLength))return BR_COMMIT_IO;
  if(phase)*phase=BR_COMMIT_PHASE_WRITE_CANDIDATE;
  if(!store.write(id,target,encoded,encodedLength))return BR_COMMIT_IO;
  if(phase)*phase=BR_COMMIT_PHASE_VERIFY_READ;
  uint8_t *checkBytes=target=='a'?b:a;int checkSize=store.read(id,target,checkBytes,capacity);
  if(checkSize<0||checkSize>(int)capacity)return BR_COMMIT_VERIFY;
  if(phase)*phase=BR_COMMIT_PHASE_VERIFY_CRC;
  BrSlotView check;if(!br_decode_slot(checkBytes,(size_t)checkSize,check)||check.generation!=next||check.length!=length||memcmp(check.payload,payload,length))return BR_COMMIT_VERIFY;
  revision=next;return BR_COMMIT_OK;
}

template<class Store>
static BrDeleteResult br_delete_slot(Store &store,uint8_t id,uint32_t expected,uint8_t *a,uint8_t *b,size_t capacity){
  if(id>7||!a||!b)return BR_DELETE_IO;
  int ar=store.read(id,'a',a,capacity),br=store.read(id,'b',b,capacity);
  if(ar==-2||br==-2||ar>(int)capacity||br>(int)capacity)return BR_DELETE_IO;
  size_t an=ar<0?0:(size_t)ar,bn=br<0?0:(size_t)br;
  BrSlotView av,bv,current;bool oka=br_decode_slot(a,an,av),okb=br_decode_slot(b,bn,bv);
  BrSlotState state=br_select_slot(a,an,b,bn,current);
  if(state==BR_SLOT_CORRUPT)return BR_DELETE_CORRUPT;
  uint32_t revision=state==BR_SLOT_EMPTY?0:current.generation;
  if(revision!=expected)return BR_DELETE_CONFLICT;
  if(state==BR_SLOT_EMPTY)return BR_DELETE_OK;
  char selected='a';
  if(!oka)selected='b';else if(!okb)selected='a';else if(bv.generation>av.generation)selected='b';
  const char first=selected=='a'?'b':'a';
  if(!store.remove(id,first))return BR_DELETE_IO;
  if(!store.remove(id,selected))return BR_DELETE_IO;
  return BR_DELETE_OK;
}

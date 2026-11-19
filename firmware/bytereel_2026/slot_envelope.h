#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static const uint32_t BR_SLOT_MAX_PAYLOAD=12288;
enum BrSlotState:uint8_t{BR_SLOT_EMPTY,BR_SLOT_VALID,BR_SLOT_DEGRADED,BR_SLOT_CORRUPT};
enum BrRevisionStatus:uint8_t{BR_REVISION_OK,BR_REVISION_CONFLICT,BR_REVISION_CORRUPT,BR_REVISION_EXHAUSTED};
struct BrSlotView{const uint8_t*payload=nullptr;uint32_t length=0,generation=0;};
static uint32_t br_crc32(const uint8_t*p,size_t n,uint32_t crc=0xffffffffu){while(n--){crc^=*p++;for(unsigned k=0;k<8;k++)crc=(crc>>1)^(0xedb88320u&-(crc&1u));}return crc;}
static void br_put16(uint8_t*p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}static void br_put32(uint8_t*p,uint32_t v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint16_t br_get16(const uint8_t*p){return p[0]|((uint16_t)p[1]<<8);}static uint32_t br_get32(const uint8_t*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static bool br_encode_slot(const uint8_t*payload,uint32_t len,uint32_t generation,uint8_t*out,size_t cap,size_t&outLen){
  if(!payload||!out||len>BR_SLOT_MAX_PAYLOAD||generation==0||cap<32u+len)return false;
  memset(out,0,32);memcpy(out,"BR26SLT",7);br_put16(out+8,1);br_put16(out+10,2);br_put32(out+12,generation);br_put32(out+16,len);memcpy(out+32,payload,len);
  uint32_t crc=br_crc32(out,32+len)^0xffffffffu;br_put32(out+20,crc);outLen=32+len;return true;
}
static bool br_decode_slot(const uint8_t*data,size_t size,BrSlotView&view){
  view=BrSlotView{};if(!data||size<32||memcmp(data,"BR26SLT\0",8)||br_get16(data+8)!=1||br_get16(data+10)!=2)return false;
  uint32_t gen=br_get32(data+12),len=br_get32(data+16);if(!gen||len>BR_SLOT_MAX_PAYLOAD||size!=32u+len)return false;
  for(unsigned i=24;i<32;i++)if(data[i])return false;
  uint32_t expected=br_get32(data+20);uint32_t crc=br_crc32(data,20);uint8_t zero[4]={0};crc=br_crc32(zero,4,crc);crc=br_crc32(data+24,size-24,crc)^0xffffffffu;if(crc!=expected)return false;
  view.payload=data+32;view.length=len;view.generation=gen;return true;
}
static BrSlotState br_select_slot(const uint8_t*a,size_t an,const uint8_t*b,size_t bn,BrSlotView&out){
  BrSlotView av,bv;bool aa=br_decode_slot(a,an,av),bb=br_decode_slot(b,bn,bv);out=BrSlotView{};
  if(!aa&&!bb)return (an==0&&bn==0)?BR_SLOT_EMPTY:BR_SLOT_CORRUPT;
  if(aa&&!bb){out=av;return bn?BR_SLOT_DEGRADED:BR_SLOT_VALID;}
  if(bb&&!aa){out=bv;return an?BR_SLOT_DEGRADED:BR_SLOT_VALID;}
  if(av.generation==bv.generation){if(av.length!=bv.length||memcmp(av.payload,bv.payload,av.length))return BR_SLOT_CORRUPT;out=av;return BR_SLOT_VALID;}
  out=av.generation>bv.generation?av:bv;return BR_SLOT_VALID;
}
static BrRevisionStatus br_next_revision(BrSlotState state,uint32_t current,uint32_t expected,uint32_t&next){
  if(state==BR_SLOT_CORRUPT)return BR_REVISION_CORRUPT;
  uint32_t actual=state==BR_SLOT_EMPTY?0:current;
  if(expected!=actual)return BR_REVISION_CONFLICT;
  if(actual==0xffffffffu)return BR_REVISION_EXHAUSTED;
  next=actual+1;return BR_REVISION_OK;
}

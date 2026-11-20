#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <cstring>
#include "web_json.h"
#include "source_validation.h"

struct BrSlotUploadValidation {
  bool ok=false;
  std::string field,reason;
  uint8_t video[256]{},audio[256]{};
};

static inline bool br_slot_upload_hex(const std::string &hex,uint8_t out[256],unsigned &badAt){
  if(hex.size()!=512){badAt=(unsigned)hex.size();return false;}
  auto digit=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;if(c>='a'&&c<='f')return c-'a'+10;return -1;};
  for(unsigned i=0;i<256;i++){int a=digit(hex[i*2]),b=digit(hex[i*2+1]);if(a<0||b<0){badAt=a<0?i*2:i*2+1;return false;}out[i]=(uint8_t)((a<<4)|b);}return true;
}

static inline bool br_slot_upload_bank(const std::string &record,const char *name,uint8_t slot,uint8_t out[256],std::string &field,std::string &reason){
  size_t begin=0,end=0;std::string object,hex,source;bool isNull=false;
  field=std::string(name);if(!br_json_object_range(record,name,begin,end)){reason="missing bank object";return false;}
  object=record.substr(begin,end-begin);field=std::string(name)+".hex";
  if(!br_json_string(object,"hex",hex,isNull)||isNull){reason="expected a HEX string containing exactly 256 bytes";return false;}
  unsigned badAt=0;if(!br_slot_upload_hex(hex,out,badAt)){reason=hex.size()!=512?"expected 512 hexadecimal characters, got "+std::to_string(hex.size()):"non-HEX character at character "+std::to_string(badAt);return false;}
  field=std::string(name)+".source";if(!br_json_string(object,"source",source,isNull)){reason="expected assembler source text or null";return false;}
  bool matches=!isNull,hasMatches=false;size_t matchAt=0;if(br_json_top_field(object,"sourceMatches",matchAt)){hasMatches=br_json_boolean(object,"sourceMatches",matches);if(!hasMatches){reason="expected sourceMatches boolean";return false;}}
  if(isNull&&matches){reason="sourceMatches=true requires assembler source text";return false;}
  if(source.size()>4096){reason="source exceeds 4096 bytes";return false;}
  uint32_t length=0;bool hasLength=br_json_unsigned(object,"length",length);size_t lengthAt=0;bool hasLengthField=br_json_top_field(object,"length",lengthAt);
  if(hasLengthField&&!hasLength){if(lengthAt+4>object.size()||object.compare(lengthAt,4,"null")!=0){field=std::string(name)+".length";reason="expected integer 0..256 or null";return false;}}
  if(hasLength&&length>256){field=std::string(name)+".length";reason="program byte length must be 0..256";return false;}
  if(isNull||!matches)return true;
  BrSourceCheckResult check=br_check_source_bytes(source,name==std::string("video")?"VIDEO":"AUDIO",out);
  if(check.result==BR_SOURCE_INVALID){reason="slot "+std::to_string(slot+1)+" bank "+name+" assembler rejected source: "+check.error;return false;}
  if(check.result==BR_SOURCE_MISMATCH){char detail[128];snprintf(detail,sizeof(detail),"slot %u bank %s source/HEX mismatch at address 0x%02X expected=0x%02X supplied=0x%02X",(unsigned)slot+1,name,(unsigned)check.address,(unsigned)check.expected,(unsigned)check.supplied);reason=detail;return false;}
  if(hasLength&&length!=check.assembledLength){field=std::string(name)+".length";reason="expected assembler length "+std::to_string(check.assembledLength)+" bytes, got "+std::to_string(length);return false;}
  return true;
}

static inline BrSlotUploadValidation br_validate_slot_upload(const std::string &record,uint8_t routeSlot){
  BrSlotUploadValidation out;
  if(record.size()>12288){out.field="slot";out.reason="serialized slot exceeds 12288 bytes";return out;}
  if(record.empty()||!br_json_balanced(record)||record[0]!='{'||record.back()!='}'){out.field="slot";out.reason="expected one complete JSON object";return out;}
  uint32_t schema=0,slot=99;std::string isa,name;bool isNull=false;
  out.field="schema";if(!br_json_unsigned(record,"schema",schema)||schema!=1){out.reason="expected schema 1";return out;}
  out.field="slot";if(!br_json_unsigned(record,"slot",slot)||slot>7||slot!=routeSlot){out.reason="route uses internal index "+std::to_string(routeSlot)+" (UI numbering is 1–8); record slot must match";return out;}
  out.field="isa";if(!br_json_string(record,"isa",isa,isNull)||isNull||isa!="2.0"){out.reason="expected ISA string 2.0";return out;}
  out.field="name";if(!br_json_string(record,"name",name,isNull)||isNull||name.size()>32){out.reason="expected a string of at most 32 characters";return out;}
  if(!br_slot_upload_bank(record,"video",(uint8_t)slot,out.video,out.field,out.reason))return out;
  if(!br_slot_upload_bank(record,"audio",(uint8_t)slot,out.audio,out.field,out.reason))return out;
  out.ok=true;return out;
}

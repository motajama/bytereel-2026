#pragma once
#include "assembler.h"

enum BrSourceCheck:uint8_t { BR_SOURCE_MATCH,BR_SOURCE_MISMATCH,BR_SOURCE_INVALID };
struct BrSourceCheckResult { BrSourceCheck result;uint16_t address;uint8_t expected,supplied;unsigned assembledLength;std::string error; };

static inline BrSourceCheckResult br_check_source_bytes(const std::string&source,const char*bank,const uint8_t*supplied){
  BrSourceCheckResult out{BR_SOURCE_INVALID,0,0,0,0,{}};
  try{BrAsmResult a=br_assemble(source,bank);out.assembledLength=a.written;for(unsigned i=0;i<256;i++)if(a.bytes[i]!=supplied[i]){out.result=BR_SOURCE_MISMATCH;out.address=(uint16_t)i;out.expected=a.bytes[i];out.supplied=supplied[i];return out;}out.result=BR_SOURCE_MATCH;return out;}catch(const std::exception&e){out.error=e.what();return out;}
}

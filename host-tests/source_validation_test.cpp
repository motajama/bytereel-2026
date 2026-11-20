#include <cassert>
#include <cstdio>
#include "../firmware/bytereel_2026/source_validation.h"
#include "../firmware/bytereel_2026/slot_upload_validation.h"

int main(){
  const std::string source="FREQ 18\nWAVE 00\nVOLUME 80\nGATE 01\nJMP 00";
  BrAsmResult assembled=br_assemble(source,"AUDIO");
  BrSourceCheckResult ok=br_check_source_bytes(source,"AUDIO",assembled.bytes);
  assert(ok.result==BR_SOURCE_MATCH&&ok.assembledLength==10);
  uint8_t changed[256];memcpy(changed,assembled.bytes,sizeof(changed));changed[4]=0x40;
  BrSourceCheckResult mismatch=br_check_source_bytes(source,"AUDIO",changed);
  assert(mismatch.result==BR_SOURCE_MISMATCH&&mismatch.address==4&&mismatch.expected==0x12&&mismatch.supplied==0x40);
  BrSourceCheckResult wrongBank=br_check_source_bytes(source,"VIDEO",assembled.bytes);
  assert(wrongBank.result==BR_SOURCE_INVALID);
  BrSourceCheckResult syntax=br_check_source_bytes("PFREQ 18\nNO_SUCH_OPCODE 00","AUDIO",assembled.bytes);
  assert(syntax.result==BR_SOURCE_INVALID&&!syntax.error.empty()&&syntax.error.find("line 2:")!=std::string::npos);
  BrAsmResult repro=br_assemble("PFREQ 18\nPDUR 00\nPPLAY 00\nJMP 06","AUDIO");
  const uint8_t prefix[]={0x80,0x18,0x81,0x00,0x82,0x00,0x30,0x06};assert(!memcmp(repro.bytes,prefix,sizeof(prefix))&&repro.written==8);
  static const char digits[]="0123456789ABCDEF";std::string audioHex,videoHex;for(unsigned i=0;i<256;i++){audioHex+=digits[repro.bytes[i]>>4];audioHex+=digits[repro.bytes[i]&15];videoHex+="00";}
  std::string slot=std::string("{\"schema\":1,\"isa\":\"2.0\",\"slot\":1,\"name\":\"SLOT 2\",\"video\":{\"hex\":\"")+videoHex+"\",\"source\":null,\"sourceMatches\":false,\"length\":null},\"audio\":{\"hex\":\""+audioHex+"\",\"source\":\"PFREQ 18\\nPDUR 00\\nPPLAY 00\\nJMP 06\",\"sourceMatches\":true,\"length\":8}}";
  BrSlotUploadValidation valid=br_validate_slot_upload(slot,1);assert(valid.ok&&valid.audio[0]==0x80&&valid.audio[6]==0x30);
  std::string changedRecord=slot;size_t at=changedRecord.find(audioHex);changedRecord[at]='8';changedRecord[at+1]='1';BrSlotUploadValidation mismatchUpload=br_validate_slot_upload(changedRecord,1);assert(!mismatchUpload.ok&&mismatchUpload.field=="audio.source"&&mismatchUpload.reason.find("slot 2 bank audio")!=std::string::npos&&mismatchUpload.reason.find("address 0x00 expected=0x80 supplied=0x81")!=std::string::npos);
  changedRecord=slot;changedRecord[at]='8';changedRecord[at+1]='1';size_t flag=changedRecord.find("\"sourceMatches\":true",at);changedRecord.replace(flag,20,"\"sourceMatches\":false");assert(br_validate_slot_upload(changedRecord,1).ok);
  std::puts("PASS production source/bytecode checker: exact 256-byte padded match, first-difference address/values, invalid assembler source");
}

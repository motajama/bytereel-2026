#include <cassert>
#include <cstdio>
#include <string>
#include "../firmware/bytereel_2026/web_json.h"

int main(){
  const std::string body=R"({"expectedRevision":0,"slot":{"schema":1,"isa":"2.0","slot":1,"name":"SLOT 2","video":{"hex":"0000","source":"; fake \"slot\": 7 and \"video\": {not metadata}","length":null},"audio":{"hex":"FFFF","source":null,"length":0}}})";
  assert(br_json_balanced(body));size_t start=0,end=0;assert(br_json_object_range(body,"slot",start,end));
  const std::string record=body.substr(start,end-start);uint32_t slot=99,revision=99;assert(br_json_unsigned(record,"slot",slot)&&slot==1);assert(br_json_unsigned(body,"expectedRevision",revision)&&revision==0);
  std::string source;bool isNull=true;assert(!br_json_string(record,"video",source,isNull)); // video is an object, not a string
  assert(br_json_object_range(record,"video",start,end));const std::string video=record.substr(start,end-start);
  assert(br_json_string(video,"source",source,isNull)&&!isNull);assert(source=="; fake \"slot\": 7 and \"video\": {not metadata}");
  size_t bankStart=0,bankEnd=0;assert(br_json_object_range(record,"audio",bankStart,bankEnd));assert(br_json_string_range(record,bankStart,bankEnd,"source",source,isNull)&&isNull);
  const std::string panelRecord=R"({"slot":1,"audio":{"hex":"0000","source":"preserve me","sourceMatches":false}})";assert(br_json_object_range(panelRecord,"audio",bankStart,bankEnd));assert(br_json_string_range(panelRecord,bankStart,bankEnd,"source",source,isNull)&&!isNull&&source=="preserve me");
  assert(!br_json_unsigned(body,"slot",slot)); // wrapper slot is an object; record's numeric slot is scoped to its object
  std::string unicode=R"({"value":"caf\u00E9 \uD83D\uDE80"})";assert(br_json_string(unicode,"value",source,isNull));assert(source=="café 🚀");
  std::puts("PASS JSON field scanner: top-level scoping, escaped assembler text, nested objects, UTF-16 escapes and revision fields");
}

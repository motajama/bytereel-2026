#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

enum BrWebMethod:uint8_t { BR_WEB_GET,BR_WEB_PUT,BR_WEB_DELETE,BR_WEB_OTHER };
enum BrWebRoute:uint8_t { BR_WEB_ROUTE_NONE,BR_WEB_ROUTE_SLOT_GET,BR_WEB_ROUTE_SLOT_EXPORT,BR_WEB_ROUTE_SLOT_PUT,BR_WEB_ROUTE_SLOT_DELETE,BR_WEB_ROUTE_INVALID };
struct BrWebMatch { BrWebRoute route=BR_WEB_ROUTE_NONE;uint8_t slot=0; };
static inline BrWebMatch br_match_slot_route(BrWebMethod method,const char *uri){
  BrWebMatch out;if(!uri)return out;static const char prefix[]="/api/v1/slots/";const size_t n=sizeof(prefix)-1;if(strncmp(uri,prefix,n))return out;
  const char *p=uri+n;if(p[0]<'0'||p[0]>'7') {out.route=BR_WEB_ROUTE_INVALID;return out;}out.slot=(uint8_t)(p[0]-'0');p++;
  bool exp=false;if(!strcmp(p,"/export")){exp=true;}else if(*p!='\0'){out.route=BR_WEB_ROUTE_INVALID;return out;}
  if(method==BR_WEB_GET)out.route=exp?BR_WEB_ROUTE_SLOT_EXPORT:BR_WEB_ROUTE_SLOT_GET;
  else if(method==BR_WEB_PUT&&!exp)out.route=BR_WEB_ROUTE_SLOT_PUT;
  else if(method==BR_WEB_DELETE&&!exp)out.route=BR_WEB_ROUTE_SLOT_DELETE;
  else out.route=BR_WEB_ROUTE_INVALID;
  return out;
}

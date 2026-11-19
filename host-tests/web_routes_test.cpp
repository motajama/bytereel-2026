#include <cassert>
#include <cstdio>
#include "../firmware/bytereel_2026/web_routes.h"
int main(){
  BrWebMatch m=br_match_slot_route(BR_WEB_GET,"/api/v1/slots/0");assert(m.route==BR_WEB_ROUTE_SLOT_GET&&m.slot==0);
  m=br_match_slot_route(BR_WEB_GET,"/api/v1/slots/7/export");assert(m.route==BR_WEB_ROUTE_SLOT_EXPORT&&m.slot==7);
  m=br_match_slot_route(BR_WEB_PUT,"/api/v1/slots/4");assert(m.route==BR_WEB_ROUTE_SLOT_PUT&&m.slot==4);
  m=br_match_slot_route(BR_WEB_DELETE,"/api/v1/slots/4");assert(m.route==BR_WEB_ROUTE_SLOT_DELETE&&m.slot==4);
  assert(br_match_slot_route(BR_WEB_PUT,"/api/v1/slots/4/export").route==BR_WEB_ROUTE_INVALID);
  assert(br_match_slot_route(BR_WEB_GET,"/api/v1/slots/8").route==BR_WEB_ROUTE_INVALID);
  assert(br_match_slot_route(BR_WEB_GET,"/api/v1/slots/0/extra").route==BR_WEB_ROUTE_INVALID);
  assert(br_match_slot_route(BR_WEB_GET,"/api/v1/slots/x").route==BR_WEB_ROUTE_INVALID);
  assert(br_match_slot_route(BR_WEB_GET,"/api/v1/slots").route==BR_WEB_ROUTE_NONE);
  std::puts("PASS shared WebServer slot route matcher: GET, PUT, DELETE, export and invalid paths");
}

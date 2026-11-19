#include <cassert>
#include <cstdio>
#include <cstring>
#include "../firmware/bytereel_2026/monitor_state.h"
int main(){
  BrFetchFifo q;for(uint16_t i=0;i<50;i++){q.push(i*2,(uint8_t)i,0);q.push(i*2+1,(uint8_t)(i+1),1);}assert(q.count==48&&q.dropped==52);BrFetchEvent e;assert(q.pop(e)&&e.address==52&&e.value==26&&e.kind==0);assert(q.pop(e)&&e.address==53&&e.value==27&&e.kind==1);q.clear();assert(!q.count);
  BrLogRing log;char line[24];for(unsigned i=0;i<70;i++){snprintf(line,sizeof(line),"record-%u",i);assert(log.append(i,"PANEL",line)==i+1);}assert(log.oldest()==7&&log.nextSequence==71&&log.lost(0)&&!log.lost(6));const BrLogEntry*old=log.find(7);assert(old&&old->atMs==6&&!strcmp(old->type,"PANEL")&&!strcmp(old->text,"record-6"));assert(!log.find(6));
  BrLogCursor cursor;cursor.begin(10,11,40,log.oldest());assert(cursor.lost&&cursor.sequence==6);cursor.advance(8);assert(cursor.sequence==8);cursor.begin(11,11,0,log.oldest());assert(cursor.lost&&cursor.sequence==6);cursor.advance(7);assert(cursor.sequence==7);
  char longText[180];memset(longText,'x',sizeof(longText));longText[sizeof(longText)-1]=0;log.append(71,"WEB_MUTATION_REJECTED",longText);assert(strlen(log.find(71)->text)==95&&!strcmp(log.find(71)->type,"WEB_MUTATION_REJECTED"));
  BrRuntimeCounters stats;stats.sample(1000,500);stats.sample(2000,1500);assert(stats.measuredIps==1000);stats.requestedIps=200;assert(stats.requestedIps==200);
  std::puts("PASS monitor state: bounded fetch overflow, log cursor/gap/restart ring, text bound, requested/measured counters");
}

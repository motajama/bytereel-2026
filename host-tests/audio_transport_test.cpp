#include <cassert>
#include <cstdio>
#include "../firmware/bytereel_2026/audio_transport_stats.h"

int main(){BrAudioTransportStats s;BrAudioBlockCursor cursor;const size_t block=256;cursor.begin();
  // Inject the partial-write shape handled by the production writer.
  size_t n=64;s.recordWrite(cursor.remaining(block),n,false,false,120);cursor.advance(n,block);
  n=192;s.recordWrite(cursor.remaining(block),n,false,false,240);cursor.advance(n,block);s.recordBlock(1000,1500,cursor.complete(block));
  assert(s.partialCalls==1&&s.bytesWritten==256&&s.completeBlocks==1&&s.incompleteAttempts==0);cursor.finish();
  cursor.begin();s.recordWrite(cursor.remaining(block),128,false,false,9500);cursor.advance(128,block);s.recordBlock(2000,12000,false);
  assert(cursor.remaining(block)==128&&!cursor.complete(block));
  s.recordWrite(cursor.remaining(block),0,true,true,10000);s.recordDeadline();s.recordBlock(2000,22000,false);
  assert(s.timeouts==1&&s.deadlineExpirations==1&&s.writeErrors==1&&s.incompleteAttempts==2&&s.bytesWritten==384);
  // A following successful call resumes at the same offset; no rendered
  // samples are thrown away when the previous transport attempt timed out.
  s.recordWrite(cursor.remaining(block),128,false,false,500);cursor.advance(128,block);s.recordBlock(2000,22500,cursor.complete(block));assert(cursor.complete(block));cursor.finish();
  cursor.begin();s.recordBlock(3000,3800,true);assert(s.maxBlockPeriodUs==1000&&s.maxBlockFillUs==20500&&s.maxWriteUs==10000);
  std::puts("PASS audio transport: partial byte accounting, cursor resumes after timeout without dropping suffix, deadline distinction and maxima");}

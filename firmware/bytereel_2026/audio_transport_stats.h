#pragma once
#include <stddef.h>
#include <stdint.h>
struct BrAudioBlockCursor {
  size_t offset=0;bool pending=false;
  void begin(){offset=0;pending=true;}
  size_t remaining(size_t total)const{return offset<total?total-offset:0;}
  void advance(size_t bytes,size_t total){offset+=bytes;if(offset>total)offset=total;}
  bool complete(size_t total)const{return pending&&offset==total;}
  void finish(){offset=0;pending=false;}
};
// These counters describe software writes. A missed block deadline is not a
// hardware DMA-underrun measurement.
struct BrAudioTransportStats {
  uint32_t calls=0,bytesWritten=0,partialCalls=0,timeouts=0,deadlineExpirations=0,writeErrors=0;
  uint32_t completeBlocks=0,incompleteAttempts=0,maxWriteUs=0,maxBlockFillUs=0,maxBlockPeriodUs=0,lastBlockStartUs=0;
  void recordWrite(size_t requested,size_t written,bool timeout,bool error,uint32_t elapsedUs){++calls;bytesWritten+=(uint32_t)written;if(written&&written<requested)++partialCalls;if(timeout)++timeouts;if(error)++writeErrors;if(elapsedUs>maxWriteUs)maxWriteUs=elapsedUs;}
  void recordDeadline(){++deadlineExpirations;}
  void recordBlock(uint32_t startUs,uint32_t finishUs,bool complete){uint32_t fill=finishUs-startUs;if(fill>maxBlockFillUs)maxBlockFillUs=fill;if(complete){if(lastBlockStartUs){uint32_t period=startUs-lastBlockStartUs;if(period>maxBlockPeriodUs)maxBlockPeriodUs=period;}lastBlockStartUs=startUs;++completeBlocks;}else ++incompleteAttempts;}
};

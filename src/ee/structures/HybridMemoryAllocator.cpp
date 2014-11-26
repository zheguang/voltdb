#include <numa.h>
#include <numaif.h>

#include "HybridMemoryAllocator.h"

using namespace voltdb;

void* HybridMemoryAllocator::alloc(size_t sz, Location location) {
  int memoryNode = 1;
  switch (location) {
    case DRAM:
      memoryNode = 1;
      break;
    case NVM:
      memoryNode = 2;
      break;
  }
  return numa_alloc_onnode(sz, memoryNode);
}

void HybridMemoryAllocator::free(void* start, size_t sz) {
  numa_free(start, sz);	
}

#include <numa.h>
#include <numaif.h>

#include "HybridMemory.h"

using namespace voltdb;

void* HybridMemory::alloc(size_t sz, MEMORY_NODE_TYPE memoryNodeType) {
  int memoryNode = 0;
  switch (memoryNodeType) {
    case DRAM:
      memoryNode = 0;
      break;
    case NVM:
      memoryNode = 2;
      break;
    default:
      memoryNode = 0;
  }
  return numa_alloc_onnode(sz, memoryNode);
}

void HybridMemory::free(void* start, size_t sz) {
  numa_free(start, sz);	
}

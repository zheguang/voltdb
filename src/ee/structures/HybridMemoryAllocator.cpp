#include <numa.h>
#include <numaif.h>

#include "HybridMemoryAllocator.h"

using namespace voltdb;

void* HybridMemoryAllocator::alloc(size_t sz, MEMORY_NODE_TYPE memoryNodeType) {
  int memoryNode = 1;
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

void HybridMemoryAllocator::free(void* start, size_t sz) {
  numa_free(start, sz);	
}

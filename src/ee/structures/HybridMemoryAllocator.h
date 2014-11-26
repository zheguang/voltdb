#ifndef HYBRID_MEMORY_ALLOCATOR_H_
#define HYBRID_MEMORY_ALLOCATOR_H_

#include <cstdlib>

namespace voltdb {

  class HybridMemoryAllocator {
  public:
    enum MEMORY_NODE_TYPE {
      DRAM,
      NVM
    };
    HybridMemoryAllocator() {}
    ~HybridMemoryAllocator() {}

    void *alloc(size_t sz, MEMORY_NODE_TYPE memoryNodeType);
    void free(void* start, size_t sz);
  };

};

#endif

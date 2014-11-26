#ifndef HYBRID_MEMORY_ALLOCATOR_H_
#define HYBRID_MEMORY_ALLOCATOR_H_

#include <cstdlib>

namespace voltdb {

  class HybridMemoryAllocator {
  public:
    enum Location {
      DRAM,
      NVM,
    };
    HybridMemoryAllocator() {}
    ~HybridMemoryAllocator() {}

    void *alloc(size_t sz, Location location);
    void free(void* start, size_t sz);
  };

};

#endif

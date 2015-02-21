#ifndef HYBRID_MEMORY_ALLOCATOR_H_
#define HYBRID_MEMORY_ALLOCATOR_H_

#include <cstdlib>
#include <libxmem.h>

namespace voltdb {

  class HybridMemory {
  public:
    enum MEMORY_NODE_TYPE {
      DRAM,
      NVM
    };

    static void *alloc(size_t sz, MEMORY_NODE_TYPE memoryNodeType);
    static void free(void* start, size_t sz, MEMORY_NODE_TYPE memoryNodeType);
    static void assertAddress(void* start, MEMORY_NODE_TYPE memoryNodeType);

  private:
      HybridMemory();
      ~HybridMemory();

      static int memoryNodeOf(MEMORY_NODE_TYPE memoryNodeType);
      static xmem_classifier_t xmemClassifierOf(MEMORY_NODE_TYPE memoryNodeType);
  };

};

#endif

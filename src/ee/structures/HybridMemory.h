#ifndef HYBRID_MEMORY_ALLOCATOR_H_
#define HYBRID_MEMORY_ALLOCATOR_H_

#include <cstdlib>
#include <libxmem.h>
#include <cstddef>
#include <string>

namespace voltdb {

  class HybridMemory {
  public:
    enum MEMORY_NODE_TYPE {
      DRAM = 0,
      DRAM_SECONDARY_PRIORITY,
      DRAM_THIRD_PRIORITY,
      DRAM_FOURTH_PRIORITY,
      DRAM_FIFITH_PRIORITY,
      NVM,
      OS_HEAP,
    };

    static void *alloc(size_t sz, MEMORY_NODE_TYPE memoryNodeType);
    static void free(void* start, size_t sz, MEMORY_NODE_TYPE memoryNodeType);
    static void assertAddress(void* start, MEMORY_NODE_TYPE memoryNodeType);

    static MEMORY_NODE_TYPE indexPriorityOf(const std::string& name);
    static MEMORY_NODE_TYPE tablePriorityOf(const std::string& name);
    static MEMORY_NODE_TYPE otherPriorityOf(const std::string& name);

  private:
      HybridMemory();
      ~HybridMemory();

      static int memoryNodeOf(MEMORY_NODE_TYPE memoryNodeType);
      //static xmem_classifier_t xmemClassifierOf(MEMORY_NODE_TYPE memoryNodeType);
      static int xmemTagOf(MEMORY_NODE_TYPE memoryNodeType);
  };

};

#endif

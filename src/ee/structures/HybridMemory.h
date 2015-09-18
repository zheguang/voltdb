#ifndef HYBRID_MEMORY_ALLOCATOR_H_
#define HYBRID_MEMORY_ALLOCATOR_H_

#include <cstdlib>
#include <libxmem.h>
#include <cstddef>
#include <string>

namespace voltdb {

  typedef int MEMORY_NODE_TYPE;

  class HybridMemory {
  public:
    static void *alloc(size_t sz, MEMORY_NODE_TYPE memoryNodeType);
    static void free(void* start, size_t sz, MEMORY_NODE_TYPE memoryNodeType);

    static MEMORY_NODE_TYPE indexPriorityOf(const std::string& name);
    static MEMORY_NODE_TYPE tablePriorityOf(const std::string& name);
    static MEMORY_NODE_TYPE otherPriorityOf(const std::string& name);

  private:
      HybridMemory();
      ~HybridMemory();

      static MEMORY_NODE_TYPE xmemTagOf(const std::string& name);
  };

};

#endif

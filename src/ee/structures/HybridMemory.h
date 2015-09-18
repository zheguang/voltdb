#ifndef HYBRID_MEMORY_ALLOCATOR_H_
#define HYBRID_MEMORY_ALLOCATOR_H_

#include <cstdlib>
#include <libxmem.h>
#include <cstddef>
#include <string>

namespace voltdb {

  typedef int tag_t;

  class HybridMemory {
  public:

    static void *alloc(size_t sz, const tag_t& tag);
    static void free(void* start, const tag_t& tag);

    static tag_t indexPriorityOf(const std::string& name);
    static tag_t tablePriorityOf(const std::string& name);
    static tag_t otherPriorityOf(const std::string& name);

    static std::string getXmemTagsString();
    static void clearXmemTags();
    static void printXmemTagsString();

  private:
      HybridMemory();
      ~HybridMemory();

      static int xmemTagOf(const std::string& name);
  };

};

#endif

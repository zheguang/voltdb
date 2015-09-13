#include <numa.h>
#include <numaif.h>
#include <cstdlib>
#include <map>
#include <string>

#include "HybridMemory.h"
#include "common/FatalException.hpp"
#include "common/debuglog.h"

#define PAGE_SIZE (1 << 12)
#define PAGE_MASK ~(PAGE_SIZE - 1)

using std::map;
using std::string;

using namespace voltdb;

map<string,int> xmemTags;
int nextTag = 0;

void* HybridMemory::alloc(size_t sz, const tag_t& tag) {
  void* result;
  switch (tag) {
    case OS_HEAP:
      result = std::malloc(sz);
      if (!result) {
        throwFatalException("Cannot allocate using malloc.");
      }
      break;
    default:
      result = xmalloc(tag, sz);
      if (!result) {
        throwFatalException("Cannot allocate using xmalloc.");
      }
  }
  return result;
}

void HybridMemory::free(void* start, const tag_t& tag) {
  switch (tag) {
    case OS_HEAP:
      std::free(start);
      break;
    default:
      xfree(start);
  }
}

int HybridMemory::xmemTagOf(const string& name) {
  if (xmemTags.find(name) == xmemTags.end()) {
    xmemTags[name] = nextTag;
    nextTag++;
  }
  return xmemTags[name];
}

tag_t HybridMemory::tablePriorityOf(const std::string& name) {
  return xmemTagOf(name);
}

tag_t HybridMemory::indexPriorityOf(const std::string& name) {
  return xmemTagOf(name);
}

tag_t HybridMemory::otherPriorityOf(const std::string& name) {
  tag_t result;
  if (name.compare("tempTable") == 0) {
    result = OS_HEAP;
  } else if (name.compare("tempPool") == 0) {
    result = OS_HEAP;
  } else if (name.compare("stringValue") == 0) {
    result = OS_HEAP;
  } else if (name.compare("binaryValue") == 0) {
    result = OS_HEAP;
  } else if (name.compare("miscel") == 0) {
    result = OS_HEAP;
  } else {
    throwFatalException("unsupported name: %s\n", name.c_str());
  }
  return result;
}

string HybridMemory::getXmemTagsString() {
  const size_t buffer_len = 2048;
  char buffer[buffer_len];
  for (map<string,int>::const_iterator it = xmemTags.begin(); it != xmemTags.end(); it++) {
    assert(strlen(buffer) < buffer_len);
    char* next_loc = buffer + strlen(buffer);
    size_t remain_len = buffer_len - strlen(buffer);
    snprintf(next_loc, remain_len, "%d -> %s\n", it->second, it->first.c_str());
  }

  return string(buffer);
}

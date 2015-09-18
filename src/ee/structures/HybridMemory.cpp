#include <numa.h>
#include <numaif.h>
#include <cstdlib>
#include <string>

#include "HybridMemory.h"
#include "common/FatalException.hpp"
#include "common/debuglog.h"

#define PAGE_SIZE (1 << 12)
#define PAGE_MASK ~(PAGE_SIZE - 1)

using std::string;

using namespace voltdb;

static const int MAX_NUM_XMEM_TAGS = 128;
static string g_xmemTags[MAX_NUM_XMEM_TAGS];
static int g_numXmemTags = 0;
static const tag_t OS_HEAP = -2;

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

tag_t HybridMemory::xmemTagOf(const string& name) {
  /*if (g_xmemTags.find(name) == g_xmemTags.end()) {
    g_xmemTags[name] = nextTag;
    nextTag++;
  }
  return g_xmemTags[name];*/
  for (int i = 0; i < g_numXmemTags; i++) {
    if (g_xmemTags[i].compare(name) == 0) {
      return i;
    }
  }
  assert(g_numXmemTags < MAX_NUM_XMEM_TAGS);
  g_xmemTags[g_numXmemTags] = name;
  tag_t result = g_numXmemTags;
  g_numXmemTags++;
  return result;
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
  } else if (name.compare("arrayValue") == 0) {
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
  //for (map<string,int>::const_iterator it = g_xmemTags.begin(); it != g_xmemTags.end(); it++) {
  for (int i = 0; i < g_numXmemTags; i++) {
    assert(strlen(buffer) < buffer_len);
    char* next_loc = buffer + strlen(buffer);
    size_t remain_len = buffer_len - strlen(buffer);
    snprintf(next_loc, remain_len, "%d -> %s\n", i, g_xmemTags[i].c_str());
  }

  return string(buffer);
}

void HybridMemory::printXmemTagsString() {
  for (int i = 0; i < g_numXmemTags; i++) {
    printf("%d -> %s\n", i, g_xmemTags[i].c_str());
  }
}


void HybridMemory::clearXmemTags() {
  g_numXmemTags = 0;
}

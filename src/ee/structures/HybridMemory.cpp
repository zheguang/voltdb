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

using namespace voltdb;
using std::map;
using std::string;

static const int MAX_NUM_TAGS = 128;
static string g_xmemTags[MAX_NUM_TAGS];
static int g_numXmemTags = 0;
static const MEMORY_NODE_TYPE OS_HEAP = -2;

void* HybridMemory::alloc(size_t sz, MEMORY_NODE_TYPE memoryNodeType) {
  void* result;
  switch (memoryNodeType) {
    case OS_HEAP:
      result = std::malloc(sz);
      if (!result) {
        throwFatalException("Cannot allocate using malloc.");
      }
      break;
    default:
      result = std::malloc(sz);
      //result = xmalloc(memoryNodeType, sz);
      if (!result) {
        throwFatalException("Cannot allocate using xmalloc.");
      }
  }
  return result;
}

void HybridMemory::free(void* start, size_t sz, MEMORY_NODE_TYPE memoryNodeType) {
  switch (memoryNodeType) {
    case OS_HEAP:
      std::free(start);
      break;
    default:
      std::free(start);
      //xfree(start);
  }
}

MEMORY_NODE_TYPE HybridMemory::xmemTagOf(const std::string& name) {
  /*if (g_xmemTags.find(name) == g_xmemTags.end()) {
    g_xmemTags[name] = (int) g_xmemTags.size();
  }*/
  for (int i = 0; i < g_numXmemTags; i++) {
    if (g_xmemTags[i].compare(name) == 0) {
      return i;
    }
  }
  g_xmemTags[g_numXmemTags] = name;
  MEMORY_NODE_TYPE result = g_numXmemTags;
  g_numXmemTags++;
  return result;
  //return OS_HEAP;
}

/*xmem_classifier_t HybridMemory::xmemClassifierOf(MEMORY_NODE_TYPE memoryNodeType) {
  xmem_classifier_t classifier;
  switch (memoryNodeType) {
    case DRAM:
      classifier = XMEM_CLASSIFIER(0, XMEM_T_NONE, XMEM_P_NONE);
      break;
    case DRAM_SECONDARY_PRIORITY:
      classifier = XMEM_CLASSIFIER(1, XMEM_T_NONE, XMEM_P_NONE);
      break;
    case DRAM_THIRD_PRIORITY:
      classifier = XMEM_CLASSIFIER(2, XMEM_T_NONE, XMEM_P_NONE);
      break;
    case DRAM_FOURTH_PRIORITY:
      classifier = XMEM_CLASSIFIER(3, XMEM_T_NONE, XMEM_P_NONE);
      break;
    case DRAM_FIFITH_PRIORITY:
      classifier = XMEM_CLASSIFIER(4, XMEM_T_NONE, XMEM_P_NONE);
      break;
    case NVM:
      classifier = XMEM_CLASSIFIER(5, XMEM_T_NONE, XMEM_P_NONE);
      break;
    default:
      throwFatalException("Non supported memory node type");
  }
  return classifier;
}*/


MEMORY_NODE_TYPE HybridMemory::tablePriorityOf(const std::string& name) {
  /*MEMORY_NODE_TYPE priority;
  if (name.compare("CUSTOMER_NAME") == 0) {
    priority = NVM;
  } else if (name.compare("STOCK") == 0) {
    priority = DRAM_FOURTH_PRIORITY;
  } else {
    priority = DRAM_SECONDARY_PRIORITY;
  }
  //fprintf(stderr, "Got table priority of (%s) as (%d).\n", name.c_str(), priority);
  return priority;*/
  return xmemTagOf(name);
}

MEMORY_NODE_TYPE HybridMemory::indexPriorityOf(const std::string& name) {
  /*MEMORY_NODE_TYPE priority;
  if (name.find("TREE") != std::string::npos) {
    priority = DRAM_SECONDARY_PRIORITY;
  } else if (name.find("HASH") != std::string::npos) {
    priority = DRAM_THIRD_PRIORITY;
  } else {
    priority = DRAM_FOURTH_PRIORITY;
    fprintf(stderr, "Got index priority of (%s) as (%d).\n", name.c_str(), priority);
  }
  //fprintf(stderr, "Got index priority of (%s) as (%d).\n", name.c_str(), priority);
  return priority;*/
  return xmemTagOf(name);
}

MEMORY_NODE_TYPE HybridMemory::otherPriorityOf(const std::string& name) {
  MEMORY_NODE_TYPE priority;
  if (name.compare("tempTable") == 0) {
    priority = OS_HEAP;
  } else if (name.compare("tempPool") == 0) {
    priority = OS_HEAP;
  } else if (name.compare("stringValue") == 0) {
    priority = OS_HEAP;
  } else if (name.compare("binaryValue") == 0) {
    priority = OS_HEAP;
  } else if (name.compare("arrayValue") == 0) {
    priority = OS_HEAP;
  } else if (name.compare("miscel") == 0) {
    priority = OS_HEAP;
  } else {
    throwFatalException("unsupported priority type: %s\n", name.c_str());
  }
  //fprintf(stderr, "Got index priority of (%s) as (%d).\n", name.c_str(), priority);
  return priority;
}

string HybridMemory::getXmemTagsString() {
  const size_t buffer_len = 2048;
  char buffer[buffer_len];
  //for (map<string,int>::const_iterator it = xmemTags.begin(); it != xmemTags.end(); it++) {
  for (int i = 0; i < g_numXmemTags; i++) {
    assert(strlen(buffer) < buffer_len);
    char* next_loc = buffer + strlen(buffer);
    size_t remain_len = buffer_len - strlen(buffer);
    snprintf(next_loc, remain_len, "%d -> %s\n", i, g_xmemTags[i].c_str());
  }

  return string(buffer);
}

void HybridMemory::clearXmemTags() {
  g_numXmemTags = 0;
}


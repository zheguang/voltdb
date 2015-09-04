#include <numa.h>
#include <numaif.h>
#include <cstdlib>

#include "HybridMemory.h"
#include "common/FatalException.hpp"
#include "common/debuglog.h"

#define PAGE_SIZE (1 << 12)
#define PAGE_MASK ~(PAGE_SIZE - 1)

using namespace voltdb;

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
      result = xmalloc(xmemTagOf(memoryNodeType), sz);
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
      xfree(start);
  }
}

void HybridMemory::assertAddress(void* start, MEMORY_NODE_TYPE memoryNodeType) {
  int numObjs = 1;
  int status[numObjs];
  void *page[numObjs];
  page[0] = (void *)((unsigned long)start & PAGE_MASK);

  long rc = move_pages(0, numObjs, page, NULL, status, MPOL_MF_MOVE); // Get the status on current node.

  if (rc) {
    throwFatalException("Unexpected error in assert address: cannot move page.");
  }
  int memoryNode = memoryNodeOf(memoryNodeType);
  if (status[0] != memoryNode) {
    throwFatalException("Address error: not on the expected memory node. Expected: %d, actual: %d. Poiter: %p. Page: %p\n.", memoryNode, status[0], start, page[0]);
  }
}

int HybridMemory::xmemTagOf(MEMORY_NODE_TYPE memoryNodeType) {
  int tag;
  switch (memoryNodeType) {
    case DRAM:
      tag = XMEM_AUTO(0);
      break;
    case DRAM_SECONDARY_PRIORITY:
      tag = XMEM_AUTO(1);
      break;
    case DRAM_THIRD_PRIORITY:
      tag = XMEM_AUTO(2);
      break;
    case DRAM_FOURTH_PRIORITY:
      tag = XMEM_AUTO(3);
      break;
    case DRAM_FIFITH_PRIORITY:
      tag = XMEM_AUTO(4);
      break;
    case NVM:
      tag = XMEM_AUTO(5);
      break;
    default:
      throwFatalException("Non supported memory node type");
  }
  return tag;
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


HybridMemory::MEMORY_NODE_TYPE HybridMemory::tablePriorityOf(const std::string& name) {
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
  return DRAM_FIFITH_PRIORITY;
}

HybridMemory::MEMORY_NODE_TYPE HybridMemory::indexPriorityOf(const std::string& name) {
  MEMORY_NODE_TYPE priority;
  if (name.find("TREE") != std::string::npos) {
    priority = DRAM_SECONDARY_PRIORITY;
  } else if (name.find("HASH") != std::string::npos) {
    priority = DRAM_THIRD_PRIORITY;
  } else {
    priority = DRAM_FOURTH_PRIORITY;
    fprintf(stderr, "Got index priority of (%s) as (%d).\n", name.c_str(), priority);
  }
  //fprintf(stderr, "Got index priority of (%s) as (%d).\n", name.c_str(), priority);
  return priority;
}

HybridMemory::MEMORY_NODE_TYPE HybridMemory::otherPriorityOf(const std::string& name) {
  MEMORY_NODE_TYPE priority;
  if (name.compare("tempTable") == 0) {
    priority = OS_HEAP;
  } else if (name.compare("tempPool") == 0) {
    priority = OS_HEAP;
  } else if (name.compare("stringValue") == 0) {
    priority = OS_HEAP;
  } else if (name.compare("binaryValue") == 0) {
    priority = OS_HEAP;
  } else {
    throwFatalException("unsupported priority type: %s\n", name.c_str());
  }
  //fprintf(stderr, "Got index priority of (%s) as (%d).\n", name.c_str(), priority);
  return priority;
}


int HybridMemory::memoryNodeOf(MEMORY_NODE_TYPE memoryNodeType) {
  int memoryNode = 0;
  switch (memoryNodeType) {
    case DRAM:
      memoryNode = 0;
      break;
    case NVM:
      memoryNode = 2;
      break;
    default:
      throwFatalException("Non supported memory node type");
  }
  return memoryNode;
}

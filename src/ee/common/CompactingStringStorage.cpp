/* This file is part of VoltDB.
 * Copyright (C) 2008-2014 VoltDB Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with VoltDB.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "common/CompactingStringStorage.h"

#include "common/FatalException.hpp"
#include "common/ThreadLocalPool.h"
#include <boost/unordered_map.hpp>
#include <iostream>

using namespace voltdb;
using namespace std;
using boost::shared_ptr;

typedef boost::shared_ptr<CompactingStringPool> PoolPtrType;
typedef boost::unordered_map<size_t, PoolPtrType> MapType;

CompactingStringStorage::CompactingStringStorage()
{
}

CompactingStringStorage::~CompactingStringStorage()
{
}

PoolPtrType CompactingStringStorage::get(size_t size, HybridMemory::MEMORY_NODE_TYPE memoryNodeType) {
    size_t alloc_size = ThreadLocalPool::getAllocationSizeForObject(size);
    if (alloc_size == 0)
    {
        throwFatalException("Attempted to allocate an object then the 1 meg limit. Requested size was %d",
            static_cast<int32_t>(size));
    }
    return getExact(alloc_size, memoryNodeType);
}

PoolPtrType CompactingStringStorage::getExact(size_t size, HybridMemory::MEMORY_NODE_TYPE memoryNodeType) {
    MapType *poolMap = getPoolMapFrom(memoryNodeType);
    MapType::iterator iter = poolMap->find(size);
    int32_t ssize = static_cast<int32_t>(size);
    PoolPtrType pool;
    if (iter == poolMap->end()) {
        // compute num_elements to be closest multiple
        // leading to a 2Meg buffer
        int32_t num_elements = (2 * 1024 * 1024 / ssize) + 1;
        pool = PoolPtrType(new CompactingStringPool(ssize, num_elements, memoryNodeType));
        poolMap->insert(pair<size_t, PoolPtrType>(size, pool));
    }
    else
    {
        pool = iter->second;
    }
    return pool;
}

MapType *CompactingStringStorage::getPoolMapFrom(HybridMemory::MEMORY_NODE_TYPE memoryNodeType) {
  switch (memoryNodeType) {
    case HybridMemory::DRAM:
      return &m_dramPoolMap;
    case HybridMemory::NVM:
      return &m_nvmPoolMap;
    default:
      throwFatalException("Unsupported memory ndoe type.");
      return NULL;
  };
}

size_t CompactingStringStorage::getPoolAllocationSize()
{
    size_t total = 0;
    for (MapType::iterator iter = m_dramPoolMap.begin();
         iter != m_dramPoolMap.end();
         ++iter)
    {
        total += iter->second->getBytesAllocated();
    }
    for (MapType::iterator iter = m_nvmPoolMap.begin();
         iter != m_nvmPoolMap.end();
         ++iter)
    {
        total += iter->second->getBytesAllocated();
    }
    return total;
}

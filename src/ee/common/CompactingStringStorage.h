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

#ifndef _EE_COMMON_COMPACTINGSTRINGSTORAGE_H_
#define _EE_COMMON_COMPACTINGSTRINGSTORAGE_H_

#include "CompactingStringPool.h"
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

namespace voltdb {

    class CompactingStringStorage {
    public:
        CompactingStringStorage():
          m_poolsMap()
          {}

        ~CompactingStringStorage() {}

        boost::shared_ptr<CompactingStringPool> get(size_t size, const tag_t& tag);

        boost::shared_ptr<CompactingStringPool> getExact(size_t size, const tag_t& tag);

        std::size_t getPoolAllocationSize();

    private:
        boost::unordered_map<size_t, boost::shared_ptr<CompactingStringPool> > *getPoolMapFrom(const tag_t& tag);

      //DRAM = 0,
      //DRAM_SECONDARY_PRIORITY,
      //DRAM_THIRD_PRIORITY,
      //DRAM_FOURTH_PRIORITY,
      //DRAM_FIFITH_PRIORITY,
      //NVM,
        boost::unordered_map<tag_t,boost::unordered_map<size_t, boost::shared_ptr<CompactingStringPool> > > m_poolsMap;
    };
}

#endif /* COMPACTINGSTRINGSTORAGE_H_ */

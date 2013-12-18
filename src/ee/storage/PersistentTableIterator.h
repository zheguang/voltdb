/* This file is part of VoltDB.
 * Copyright (C) 2008-2013 VoltDB Inc.
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

#ifndef VOLTPERSISTENTTABLEITERATOR_H
#define VOLTPERSISTENTTABLEITERATOR_H

#include <cassert>
#include "storage/tableiterator.h"

namespace voltdb {
/**
 * Iterator for table which neglects deleted tuples.
 * TableIterator is a small and copiable object.
 * You can copy it, not passing a pointer of it.
 *
 * This class should be a virtual interface or should
 * be templated on the underlying table data iterator.
 * Either change requires some updating of the iterators
 * that are persvasively stack allocated...
 *
 */
class PersistentTableIterator : public TableIterator {
public:
    // Get an iterator via table->iterator()
    PersistentTableIterator(Table *, TBMapI);
    void reset(TBMapI);

    /**
     * Updates the given tuple so that it points to the next tuple in the table.
     * @param out the tuple will point to the retrieved tuple if this method returns true.
     * @return true if succeeded. false if no more active tuple is there.
    */
    bool next(TableTuple &out);

private:
    /*
     * Configuration parameter that controls whether the table iterator
     * stops when it has found the expected number of tuples or when it has iterated
     * all the blocks. The former is able to stop sooner without having to read to the end of
     * of the block. The latter is useful when the table will be modified after the creation of
     * the iterator. It is assumed that the code invoking this iterator is handling
     * the modifications that occur after the iterator is created.
     *
     * When set to false the counting of found tuples method is used. When set to true
     * all blocks are scanned.
     */
    TBMapI m_blockIterator;
};

inline PersistentTableIterator::PersistentTableIterator(Table *parent, TBMapI start)
    : TableIterator(parent)
    {
        m_blockIterator = start;
    }


inline void PersistentTableIterator::reset(TBMapI start) {
    TableIterator::reset();
    m_blockIterator = start;
}

inline bool PersistentTableIterator::next(TableTuple &out) {
    while (m_foundTuples < m_activeTuples) {
        if (m_currentBlock == NULL ||
            m_blockOffset >= m_currentBlock->unusedTupleBoundry()) {
//            assert(m_blockIterator != m_table->m_data.end());
//            if (m_blockIterator == m_table->m_data.end()) {
//                throwFatalException("Could not find the expected number of tuples during a table scan");
//            }
            m_dataPtr = m_blockIterator.key();
            m_currentBlock = m_blockIterator.data();
            m_blockOffset = 0;
            m_blockIterator++;
        } else {
            m_dataPtr += m_tupleLength;
        }
        assert (out.sizeInValues() == m_table->columnCount());
        out.move(m_dataPtr);
        assert(m_dataPtr < m_currentBlock.get()->address() + m_table->m_tableAllocationTargetSize);
        assert(m_dataPtr < m_currentBlock.get()->address() + (m_table->m_tupleLength * m_table->m_tuplesPerBlock));
        //assert(m_foundTuples == m_location);
        ++m_location;
        ++m_blockOffset;

        //assert(out.isActive());

        const bool active = out.isActive();
        const bool pendingDelete = out.isPendingDelete();
        const bool isPendingDeleteOnUndoRelease = out.isPendingDeleteOnUndoRelease();
        // Return this tuple only when this tuple is not marked as deleted.
        if (active) {
            ++m_foundTuples;
            if (!(pendingDelete || isPendingDeleteOnUndoRelease)) {
                //assert(m_foundTuples == m_location);
                return true;
            }
        }
    }
    return false;
}

}

#endif

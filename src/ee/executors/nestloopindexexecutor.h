/* This file is part of VoltDB.
 * Copyright (C) 2008-2010 VoltDB L.L.C.
 *
 * This file contains original code and/or modifications of original code.
 * Any modifications made by VoltDB L.L.C. are licensed under the following
 * terms and conditions:
 *
 * VoltDB is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VoltDB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VoltDB.  If not, see <http://www.gnu.org/licenses/>.
 */
/* Copyright (C) 2008 by H-Store Project
 * Brown University
 * Massachusetts Institute of Technology
 * Yale University
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef HSTORENESTLOOPINDEXEXECUTOR_H
#define HSTORENESTLOOPINDEXEXECUTOR_H

#include "common/common.h"
#include "common/valuevector.h"
#include "common/tabletuple.h"
#include "expressions/abstractexpression.h"
#include "executors/abstractexecutor.h"


namespace voltdb {

class NestLoopIndexPlanNode;
class IndexScanPlanNode;
class PersistentTable;
class Table;
class TempTable;
class TableIndex;

/**
 * Nested loop for IndexScan.
 * This is the implementation of usual nestloop which receives
 * one input table (<i>outer</i> table) and repeatedly does indexscan
 * on another table (<i>inner</i> table) with inner table's index.
 * This executor is faster than HashMatchJoin and MergeJoin if only one
 * of underlying tables has low selectivity.
 */
class NestLoopIndexExecutor : public AbstractExecutor
{
public:
    NestLoopIndexExecutor(VoltDBEngine *engine, AbstractPlanNode* abstract_node)
        : AbstractExecutor(engine, abstract_node),
        index_values_backing_store(NULL)
    {
        node = NULL;
        inline_node = NULL;
        output_table = NULL;
        inner_table = NULL;
        index = NULL;
        outer_table = NULL;
        m_lookupType = INDEX_LOOKUP_TYPE_INVALID;
    }

    ~NestLoopIndexExecutor();

protected:
    bool p_init(AbstractPlanNode*, const catalog::Database* catalog_db, int* tempTableMemoryInBytes);
    bool p_execute(const NValueArray &params);

    NestLoopIndexPlanNode* node;
    IndexScanPlanNode* inline_node;
    IndexLookupType m_lookupType;
    TempTable* output_table;
    PersistentTable* inner_table;
    TableIndex *index;
    TableTuple index_values;
    Table* outer_table;
    JoinType join_type;
    std::vector<AbstractExpression*> m_outputExpressions;

    //So valgrind doesn't report the data as lost.
    char *index_values_backing_store;
};

}

#endif

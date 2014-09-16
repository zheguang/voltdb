/* This file is part of VoltDB.
 * Copyright (C) 2008-2014 VoltDB Inc.
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
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Initializes the database, pushing the list of contestants and documenting domain data (Area codes and States).
//

package jittester.procedures;

import org.voltdb.ProcInfo;
import org.voltdb.SQLStmt;
import org.voltdb.VoltProcedure;

public class Initialize extends VoltProcedure
{
    // Check if the database has already been initialized
    public final SQLStmt checkStmt = new SQLStmt("SELECT COUNT(*) FROM my_table;");

    // Inserts an area code/state mapping
    public final SQLStmt insertStmt = new SQLStmt("INSERT INTO my_table VALUES (?,?);");

    public long run() {

        voltQueueSQL(checkStmt, EXPECT_SCALAR_LONG);
        long existingRows = voltExecuteSQL()[0].asScalarLong();

        // if the data is initialized, return the contestant count
        if (existingRows != 0)
            return existingRows;

        // initialize the data
        int rowsInserted = 0;
        final int batchSize = 16;
        for (int i=0; i < 1024 * 4; i++) {
            voltQueueSQL(insertStmt, EXPECT_SCALAR_MATCH(1), i, i);
            rowsInserted++;
            if (i % 100 == 0) {
                System.out.println("i = " + i);
            }
            if (i % batchSize == batchSize - 1)
                voltExecuteSQL();

        }

        return rowsInserted;
    }
}

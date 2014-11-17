package org.voltdb.sysprocs;

import org.voltdb.*;

import java.util.List;
import java.util.Map;

import static org.voltdb.VoltTable.ColumnInfo;

@ProcInfo(singlePartition = false)
public class SamBenchmark extends VoltSystemProcedure {

    public VoltTable[] run(SystemProcedureExecutionContext ctx)
    {
        VoltTable table = new VoltTable(new ColumnInfo("Result", VoltType.STRING));

        ctx.getSiteProcedureConnection().printBench();

        return (new VoltTable[] {table});
    }

    @Override
    public void init() {
    }

    @Override
    public DependencyPair executePlanFragment(Map<Integer, List<VoltTable>> dependencies, long fragmentId, ParameterSet params, SystemProcedureExecutionContext context) {
        context.getSiteProcedureConnection().printBench();
        return null;
    }
}


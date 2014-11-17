package org.voltdb.sysprocs;

import org.voltdb.*;

import java.util.List;
import java.util.Map;

import static org.voltdb.VoltTable.ColumnInfo;
import static org.voltdb.sysprocs.SamBenchmark.Command.commandFor;

@ProcInfo(singlePartition = false)
public class SamBenchmark extends VoltSystemProcedure {

    public VoltTable[] run(SystemProcedureExecutionContext ctx, String commandText)
    {
        VoltTable table = new VoltTable(new ColumnInfo("Result", VoltType.STRING));
        executeCommand(ctx, commandFor(commandText));
        return (new VoltTable[] {table});
    }

    @Override
    public void init() {
    }

    @Override
    public DependencyPair executePlanFragment(Map<Integer, List<VoltTable>> dependencies, long fragmentId, ParameterSet params, SystemProcedureExecutionContext context) {
        Command command = commandFor((String) params.toArray()[0]);
        executeCommand(context, command);
        return null;
    }

    private void executeCommand(SystemProcedureExecutionContext context, Command command) {
        switch (command) {
            case PRINT:
                context.getSiteProcedureConnection().printBench();
                break;
            case CLEAR:
                context.getSiteProcedureConnection().clearBench();
                break;
            default:
                throw new RuntimeException("Unsupported command: " + command);
        }
    }

    public static enum Command {
        PRINT("print"),
        CLEAR("clear");

        private final String commandText;

        private Command(String commandText) {
            this.commandText = commandText.toLowerCase();
        }

        @Override
        public String toString() {
            return commandText;
        }

        public static Command commandFor(String commandText) {
            if (commandText.equalsIgnoreCase(PRINT.toString())) {
                return PRINT;
            } else if (commandText.equalsIgnoreCase(CLEAR.toString())) {
                return CLEAR;
            } else {
                throw new RuntimeException("Unsupported command type.");
            }
        }
    }

}


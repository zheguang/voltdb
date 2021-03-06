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

package org.voltdb.compiler;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

import org.hsqldb_voltpatches.HSQLInterface;
import org.voltdb.catalog.Catalog;
import org.voltdb.catalog.Database;
import org.voltdb.catalog.PlanFragment;
import org.voltdb.catalog.Statement;
import org.voltdb.catalog.StmtParameter;
import org.voltdb.compiler.VoltCompiler.VoltCompilerException;
import org.voltdb.compilereport.StatementAnnotation;
import org.voltdb.planner.CompiledPlan;
import org.voltdb.planner.PlanningErrorException;
import org.voltdb.planner.QueryPlanner;
import org.voltdb.planner.StatementPartitioning;
import org.voltdb.planner.TrivialCostModel;
import org.voltdb.plannodes.AbstractPlanNode;
import org.voltdb.plannodes.AbstractScanPlanNode;
import org.voltdb.plannodes.DeletePlanNode;
import org.voltdb.plannodes.InsertPlanNode;
import org.voltdb.plannodes.PlanNodeList;
import org.voltdb.plannodes.UpdatePlanNode;
import org.voltdb.types.QueryType;
import org.voltdb.utils.BuildDirectoryUtils;
import org.voltdb.utils.CatalogUtil;
import org.voltdb.utils.Encoder;

import com.google_voltpatches.common.base.Charsets;

/**
 * Compiles individual SQL statements and updates the given catalog.
 * <br/>Invokes the Optimizer to generate plans.
 *
 */
public abstract class StatementCompiler {

    public static final int DEFAULT_MAX_JOIN_TABLES = 5;

    static void compile(VoltCompiler compiler, HSQLInterface hsql,
            Catalog catalog, Database db, DatabaseEstimates estimates,
            Statement catalogStmt, String stmt, String joinOrder,
            DeterminismMode detMode, StatementPartitioning partitioning)
    throws VoltCompiler.VoltCompilerException {

        // Cleanup whitespace newlines for catalog compatibility
        // and to make statement parsing easier.
        stmt = stmt.replaceAll("\n", " ");
        stmt = stmt.trim();
        compiler.addInfo("Compiling Statement: " + stmt);

        // determine the type of the query
        QueryType qtype = QueryType.getFromSQL(stmt);

        catalogStmt.setReadonly(qtype.isReadOnly());
        catalogStmt.setQuerytype(qtype.getValue());

        // put the data in the catalog that we have
        if (!stmt.endsWith(";")) {
            stmt += ";";
        }
        catalogStmt.setSqltext(stmt);
        catalogStmt.setSinglepartition(partitioning.wasSpecifiedAsSingle());
        catalogStmt.setBatched(false);
        catalogStmt.setParamnum(0);

        String name = catalogStmt.getParent().getTypeName() + "-" + catalogStmt.getTypeName();
        String sql = catalogStmt.getSqltext();
        String stmtName = catalogStmt.getTypeName();
        String procName = catalogStmt.getParent().getTypeName();
        TrivialCostModel costModel = new TrivialCostModel();

        CompiledPlan plan = null;
        QueryPlanner planner = new QueryPlanner(
                sql, stmtName, procName,  catalog.getClusters().get("cluster"), db,
                partitioning, hsql, estimates, false, DEFAULT_MAX_JOIN_TABLES,
                costModel, null, joinOrder, detMode);
        try {
            planner.parse();
            plan = planner.plan();
            assert(plan != null);
        } catch (PlanningErrorException e) {
            // These are normal expectable errors -- don't normally need a stack-trace.
            String msg = "Failed to plan for statement (" + catalogStmt.getTypeName() + ") " + catalogStmt.getSqltext();
            if (e.getMessage() != null) {
                msg += " Error: \"" + e.getMessage() + "\"";
            }
            throw compiler.new VoltCompilerException(msg);
        }
        catch (Exception e) {
            e.printStackTrace();
            throw compiler.new VoltCompilerException("Failed to plan for stmt: " + catalogStmt.getTypeName());
        }

        // There is a hard-coded limit to the number of parameters that can be passed to the EE.
        if (plan.parameters.length > CompiledPlan.MAX_PARAM_COUNT) {
            throw compiler.new VoltCompilerException(
                "The statement's parameter count " + plan.parameters.length +
                " must not exceed the maximum " + CompiledPlan.MAX_PARAM_COUNT);
        }

        // Check order determinism before accessing the detail which it caches.
        boolean orderDeterministic = plan.isOrderDeterministic();
        catalogStmt.setIsorderdeterministic(orderDeterministic);
        boolean contentDeterministic = orderDeterministic || ! plan.hasLimitOrOffset();
        catalogStmt.setIscontentdeterministic(contentDeterministic);
        String nondeterminismDetail = plan.nondeterminismDetail();
        catalogStmt.setNondeterminismdetail(nondeterminismDetail);

        catalogStmt.setSeqscancount(plan.countSeqScans());

        // Input Parameters
        // We will need to update the system catalogs with this new information
        for (int i = 0; i < plan.parameters.length; ++i) {
            StmtParameter catalogParam = catalogStmt.getParameters().add(String.valueOf(i));
            catalogParam.setJavatype(plan.parameters[i].getValueType().getValue());
            catalogParam.setIsarray(plan.parameters[i].getParamIsVector());
            catalogParam.setIndex(i);
        }

        catalogStmt.setReplicatedtabledml(plan.replicatedTableDML);

        // output the explained plan to disk (or caller) for debugging
        StringBuilder planDescription = new StringBuilder(1000); // Initial capacity estimate.
        planDescription.append("SQL: ").append(plan.sql);
        // Only output the cost in debug mode.
        // Cost seems to only confuse people who don't understand how this number is used/generated.
        if (VoltCompiler.DEBUG_MODE) {
            planDescription.append("\nCOST: ").append(plan.cost);
        }
        planDescription.append("\nPLAN:\n");
        planDescription.append(plan.explainedPlan);
        String planString = planDescription.toString();
        BuildDirectoryUtils.writeFile(null, name + ".txt", planString, false);
        compiler.captureDiagnosticContext(planString);

        // Stuff the explain plan in an annotation for report generation.
        // N.B. The explain plan is actually in the catalog as of 5/28/13, but
        // plans are in place to remove it shortly.
        StatementAnnotation annotation = new StatementAnnotation();
        annotation.explainPlan = plan.explainedPlan;
        catalogStmt.setAnnotation(annotation);
        // build usage links for report generation
        CatalogUtil.updateUsageAnnotations(db, catalogStmt, plan.rootPlanGraph, plan.subPlanGraph);

        // set the explain plan output into the catalog (in hex)
        catalogStmt.setExplainplan(Encoder.hexEncode(plan.explainedPlan));

        // compute a hash of the plan
        MessageDigest md = null;
        try {
            md = MessageDigest.getInstance("SHA-1");
        } catch (NoSuchAlgorithmException e) {
            e.printStackTrace();
            assert(false);
            System.exit(-1); // should never happen with healthy jvm
        }

        // Now update our catalog information
        PlanFragment planFragment = catalogStmt.getFragments().add("0");
        planFragment.setHasdependencies(plan.subPlanGraph != null);
        // mark a fragment as non-transactional if it never touches a persistent table
        planFragment.setNontransactional(!fragmentReferencesPersistentTable(plan.rootPlanGraph));
        planFragment.setMultipartition(plan.subPlanGraph != null);
        byte[] planBytes = writePlanBytes(compiler, planFragment, plan.rootPlanGraph);
        md.update(planBytes, 0, planBytes.length);
        // compute the 40 bytes of hex from the 20 byte sha1 hash of the plans
        md.reset();
        md.update(planBytes);
        planFragment.setPlanhash(Encoder.hexEncode(md.digest()));

        if (plan.subPlanGraph != null) {
            planFragment = catalogStmt.getFragments().add("1");
            planFragment.setHasdependencies(false);
            planFragment.setNontransactional(false);
            planFragment.setMultipartition(true);
            byte[] subBytes = writePlanBytes(compiler, planFragment, plan.subPlanGraph);
            // compute the 40 bytes of hex from the 20 byte sha1 hash of the plans
            md.reset();
            md.update(subBytes);
            planFragment.setPlanhash(Encoder.hexEncode(md.digest()));
        }

        // Planner should have rejected with an exception any statement with an unrecognized type.
        int validType = catalogStmt.getQuerytype();
        assert(validType != QueryType.INVALID.getValue());
    }

    /**
     * Update the plan fragment and return the bytes of the plan
     */
    static byte[] writePlanBytes(VoltCompiler compiler, PlanFragment fragment, AbstractPlanNode planGraph)
    throws VoltCompilerException {
        // get the plan bytes
        PlanNodeList node_list = new PlanNodeList(planGraph);
        String json = node_list.toJSONString();
        compiler.captureDiagnosticJsonFragment(json);
        // Place serialized version of PlanNodeTree into a PlanFragment
        byte[] jsonBytes = json.getBytes(Charsets.UTF_8);
        String bin64String = Encoder.compressAndBase64Encode(jsonBytes);
        fragment.setPlannodetree(bin64String);
        return jsonBytes;
    }

    /**
     * Check through a plan graph and return true if it ever touches a persistent table.
     */
    static boolean fragmentReferencesPersistentTable(AbstractPlanNode node) {
        if (node == null)
            return false;

        // these nodes can read/modify persistent tables
        if (node instanceof AbstractScanPlanNode)
            return true;
        if (node instanceof InsertPlanNode)
            return true;
        if (node instanceof DeletePlanNode)
            return true;
        if (node instanceof UpdatePlanNode)
            return true;

        // recursively check out children
        for (int i = 0; i < node.getChildCount(); i++) {
            AbstractPlanNode child = node.getChild(i);
            if (fragmentReferencesPersistentTable(child))
                return true;
        }

        // if nothing found, return false
        return false;
    }

}

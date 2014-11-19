/* This file is part of VoltDB.
 * Copyright (C) 2008-2014 VoltDB Inc.
 *
 * This file contains original code and/or modifications of original code.
 * Any modifications made by VoltDB Inc. are licensed under the following
 * terms and conditions:
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

#ifndef VOLTDBENGINE_H
#define VOLTDBENGINE_H

#include "common/TimeMeasure.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>
#include <cassert>

#include <boost/ptr_container/ptr_vector.hpp>
#include "boost/shared_ptr.hpp"
// The next #define limits the number of features pulled into the build
// We don't use those features.
#define BOOST_MULTI_INDEX_DISABLE_SERIALIZATION
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include "catalog/database.h"
#include "common/ids.h"
#include "common/serializeio.h"
#include "common/types.h"
#include "common/valuevector.h"
#include "common/Pool.hpp"
#include "common/UndoLog.h"
#include "common/SerializableEEException.h"
#include "common/Topend.h"
#include "common/DefaultTupleSerializer.h"
#include "common/TupleOutputStream.h"
#include "common/TheHashinator.h"
#include "execution/FragmentManager.h"
#include "logging/LogManager.h"
#include "logging/LogProxy.h"
#include "logging/StdoutLogProxy.h"
#include "plannodes/plannodefragment.h"
#include "stats/StatsAgent.h"
#include "storage/TempTableLimits.h"
#include "common/ThreadLocalPool.h"

// shorthand for ExecutionEngine versions generated by javah
#define ENGINE_ERRORCODE_SUCCESS 0
#define ENGINE_ERRORCODE_ERROR 1

#define MAX_BATCH_COUNT 1000
#define MAX_PARAM_COUNT 1025 // keep in synch with value in CompiledPlan.java

namespace catalog {
class Catalog;
class PlanFragment;
class Table;
class Statement;
class Cluster;
}

class VoltDBIPC;

namespace voltdb {

class AbstractExecutor;
class AbstractPlanNode;
class SerializeInput;
class SerializeOutput;
class PersistentTable;
class Table;
class CatalogDelegate;
class TableCatalogDelegate;
class PlanNodeFragment;
class ExecutorContext;
class RecoveryProtoMsg;

const int64_t DEFAULT_TEMP_TABLE_MEMORY = 1024 * 1024 * 100;
const size_t PLAN_CACHE_SIZE = 1024 * 10;
// how many initial tuples to scan before calling into java
const int64_t LONG_OP_THRESHOLD = 10000;

struct IndexBench {
  timespec _time;
  int64_t _numCalls;
};

/**
 * Represents an Execution Engine which holds catalog objects (i.e. table) and executes
 * plans on the objects. Every operation starts from this object.
 * This class is designed to be single-threaded.
 */
// TODO(evanj): Used by JNI so must be exported. Remove when we only one .so
class __attribute__((visibility("default"))) VoltDBEngine {
    public:
        std::string asString(const std::map<std::string,IndexBench>& indexBenchMap);

        typedef std::pair<std::string, CatalogDelegate*> LabeledCDPair;

        /** Constructor for test code: this does not enable JNI callbacks. */
        VoltDBEngine() :
          m_currentIndexInBatch(0),
          m_allTuplesScanned(0),
          m_tuplesProcessedInBatch(0),
          m_tuplesProcessedInFragment(0),
          m_tuplesProcessedSinceReport(0),
          m_tupleReportThreshold(LONG_OP_THRESHOLD),
          m_lastAccessedTable(NULL),
          m_lastAccessedExec(NULL),
          m_currentUndoQuantum(NULL),
          m_hashinator(NULL),
          m_staticParams(MAX_PARAM_COUNT),
          m_currentInputDepId(-1),
          m_isELEnabled(false),
          m_numResultDependencies(0),
          m_logManager(new StdoutLogProxy()),
          m_templateSingleLongTable(NULL),
          m_topend(NULL),
          m_compactionThreshold(95),
          m_numBackendCalls(0),
          m_numIndexExecutorsCalls(0)
        {
          m_backendTime.tv_sec = 0;
          m_backendTime.tv_nsec = 0;
          m_indexExecutorsTime.tv_sec = 0;
          m_indexExecutorsTime.tv_nsec = 0;
        }

        VoltDBEngine(Topend *topend, LogProxy *logProxy);
        bool initialize(int32_t clusterIndex,
                        int64_t siteId,
                        int32_t partitionId,
                        int32_t hostId,
                        std::string hostname,
                        int64_t tempTableMemoryLimit,
                        int32_t compactionThreshold = 95);
        virtual ~VoltDBEngine();

        inline int32_t getClusterIndex() const { return m_clusterIndex; }
        inline int64_t getSiteId() const { return m_siteId; }

        // ------------------------------------------------------------------
        // OBJECT ACCESS FUNCTIONS
        // ------------------------------------------------------------------
        catalog::Catalog *getCatalog() const;

        Table* getTable(int32_t tableId) const;
        Table* getTable(std::string name) const;
        // Serializes table_id to out. Returns true if successful.
        bool serializeTable(int32_t tableId, SerializeOutput* out) const;

        TableCatalogDelegate* getTableDelegate(std::string name) const;
        catalog::Database* getDatabase() const { return m_database; }
        catalog::Table* getCatalogTable(std::string name) const;

        // -------------------------------------------------
        // Execution Functions
        // -------------------------------------------------

        /**
         * Execute a list of plan fragments, with the params yet-to-be deserialized.
         */
        int executePlanFragments(int32_t numFragments,
                                 int64_t planfragmentIds[],
                                 int64_t intputDependencyIds[],
                                 ReferenceSerializeInput &serialize_in,
                                 int64_t spHandle,
                                 int64_t lastCommittedSpHandle,
                                 int64_t uniqueId,
                                 int64_t undoToken);
        
        void printBench();

        void clearBench();

        inline int getUsedParamcnt() const { return m_usedParamcnt;}

        /** index of the batch piece being executed */
        inline int getIndexInBatch() {
            return m_currentIndexInBatch;
        }

        // Created to transition existing unit tests to context abstraction.
        // If using this somewhere new, consider if you're being lazy.
        ExecutorContext *getExecutorContext();

        // Executors can call this to note a certain number of tuples have been
        // scanned or processed.index
        inline int64_t pullTuplesRemainingUntilProgressReport(AbstractExecutor* exec, Table* target_table);
        inline int64_t pushTuplesProcessedForProgressMonitoring(int64_t tuplesProcessed);
        inline void pushFinalTuplesProcessedForProgressMonitoring(int64_t tuplesProcessed);

        // -------------------------------------------------
        // Dependency Transfer Functions
        // -------------------------------------------------
        bool send(Table* dependency);
        int loadNextDependency(Table* destination);

        // -------------------------------------------------
        // Catalog Functions
        // -------------------------------------------------
        bool loadCatalog(const int64_t timestamp, const std::string &catalogPayload);
        bool updateCatalog(const int64_t timestamp, const std::string &catalogPayload);
        bool processCatalogAdditions(bool addAll, int64_t timestamp);


        /**
        * Load table data into a persistent table specified by the tableId parameter.
        * This must be called at most only once before any data is loaded in to the table.
        */
        bool loadTable(int32_t tableId,
                       ReferenceSerializeInput &serializeIn,
                       int64_t spHandle, int64_t lastCommittedSpHandle,
                       bool returnUniqueViolations);

        void resetReusedResultOutputBuffer(const size_t headerSize = 0);
        inline ReferenceSerializeOutput* getExceptionOutputSerializer() { return &m_exceptionOutput; }
        void setBuffers(char *parameter_buffer, int m_parameterBuffercapacity,
                char *resultBuffer, int resultBufferCapacity,
                char *exceptionBuffer, int exceptionBufferCapacity);
        inline const char* getParameterBuffer() const { return m_parameterBuffer;}
        /** Returns the size of buffer for passing parameters to EE. */
        inline int getParameterBufferCapacity() const { return m_parameterBufferCapacity;}

        /**
         * Retrieves the size in bytes of the data that has been placed in the reused result buffer
         */
        int getResultsSize() const;

        /** Returns the buffer for receiving result tables from EE. */
        inline char* getReusedResultBuffer() const { return m_reusedResultBuffer;}
        /** Returns the size of buffer for receiving result tables from EE. */
        inline int getReusedResultBufferCapacity() const { return m_reusedResultCapacity;}

        NValueArray& getParameterContainer() { return m_staticParams; }
        int64_t* getBatchFragmentIdsContainer() { return m_batchFragmentIdsContainer; }
        int64_t* getBatchDepIdsContainer() { return m_batchDepIdsContainer; }

        /** are we sending tuples to another database? */
        bool isELEnabled() { return m_isELEnabled; }

        /** check if this value hashes to the local partition */
        bool isLocalSite(const NValue& value);

        // -------------------------------------------------
        // Non-transactional work methods
        // -------------------------------------------------

        /** Perform once per second, non-transactional work. */
        void tick(int64_t timeInMillis, int64_t lastCommittedSpHandle);

        /** flush active work (like EL buffers) */
        void quiesce(int64_t lastCommittedSpHandle);

        // -------------------------------------------------
        // Save and Restore Table to/from disk functions
        // -------------------------------------------------

        /**
         * Save the table specified by catalog id tableId to the
         * absolute path saveFilePath
         *
         * @param tableGuid the GUID of the table in the catalog
         * @param saveFilePath the full path of the desired savefile
         * @return true if successful, false if save failed
         */
        bool saveTableToDisk(int32_t clusterId, int32_t databaseId, int32_t tableId, std::string saveFilePath);

        /**
         * Restore the table from the absolute path saveFilePath
         *
         * @param restoreFilePath the full path of the file with the
         * table to restore
         * @return true if successful, false if save failed
         */
        bool restoreTableFromDisk(std::string restoreFilePath);

        // -------------------------------------------------
        // Debug functions
        // -------------------------------------------------
        std::string debug(void) const;

        /** Counts tuples modified by a plan fragment */
        int64_t m_tuplesModified;
        /** True if any fragments in a batch have modified any tuples */
        bool m_dirtyFragmentBatch;

        std::string m_stmtName;
        std::string m_fragName;

        std::map<std::string, int*> m_indexUsage;

        // -------------------------------------------------
        // Statistics functions
        // -------------------------------------------------
        voltdb::StatsAgent& getStatsManager();

        /**
         * Retrieve a set of statistics and place them into the result buffer as a set of VoltTables.
         * @param selector StatisticsSelectorType indicating what set of statistics should be retrieved
         * @param locators Integer identifiers specifying what subset of possible statistical sources should be polled. Probably a CatalogId
         *                 Can be NULL in which case all possible sources for the selector should be included.
         * @param numLocators Size of locators array.
         * @param interval Whether to return counters since the beginning or since the last time this was called
         * @param Timestamp to embed in each row
         * @return Number of result tables, 0 on no results, -1 on failure.
         */
        int getStats(
                int selector,
                int locators[],
                int numLocators,
                bool interval,
                int64_t now);

        inline Pool* getStringPool() { return &m_stringPool; }

        inline LogManager* getLogManager() {
            return &m_logManager;
        }

        inline void setUndoToken(int64_t nextUndoToken) {
            if (nextUndoToken == INT64_MAX) { return; }
            if (m_currentUndoQuantum != NULL) {
                if (m_currentUndoQuantum->getUndoToken() == nextUndoToken) {
                    return;
                }
                assert(nextUndoToken > m_currentUndoQuantum->getUndoToken());
            }
            setCurrentUndoQuantum(m_undoLog.generateUndoQuantum(nextUndoToken));
        }

        inline void releaseUndoToken(int64_t undoToken) {
            if (m_currentUndoQuantum != NULL && m_currentUndoQuantum->getUndoToken() == undoToken) {
                m_currentUndoQuantum = NULL;
            }
            m_undoLog.release(undoToken);
        }
        inline void undoUndoToken(int64_t undoToken) {
            m_undoLog.undo(undoToken);
            m_currentUndoQuantum = NULL;
        }

        inline voltdb::UndoQuantum* getCurrentUndoQuantum() { return m_currentUndoQuantum; }

        inline Topend* getTopend() { return m_topend; }

        /**
         * Activate a table stream of the specified type for the specified table.
         * Returns true on success and false on failure
         */
        bool activateTableStream(
                const CatalogId tableId,
                const TableStreamType streamType,
                int64_t undoToken,
                ReferenceSerializeInput &serializeIn);

        /**
         * Serialize tuples to output streams from a table in COW mode.
         * Overload that serializes a stream position array.
         * Return remaining tuple count, 0 if done, or TABLE_STREAM_SERIALIZATION_ERROR on error.
         */
        int64_t tableStreamSerializeMore(const CatalogId tableId,
                                         const TableStreamType streamType,
                                         ReferenceSerializeInput &serializeIn);

        /**
         * Serialize tuples to output streams from a table in COW mode.
         * Overload that populates a position vector provided by the caller.
         * Return remaining tuple count, 0 if done, or TABLE_STREAM_SERIALIZATION_ERROR on error.
         */
        int64_t tableStreamSerializeMore(const CatalogId tableId,
                                         const TableStreamType streamType,
                                         ReferenceSerializeInput &serializeIn,
                                         std::vector<int> &retPositions);

        /*
         * Apply the updates in a recovery message.
         */
        void processRecoveryMessage(RecoveryProtoMsg *message);

        /**
         * Perform an action on behalf of Export.
         *
         * @param if syncAction is true, the stream offset being set for a table
         * @param the catalog version qualified id of the table to which this action applies
         * @return the universal offset for any poll results (results
         * returned separatedly via QueryResults buffer)
         */
        int64_t exportAction(bool syncAction, int64_t ackOffset, int64_t seqNo, std::string tableSignature);

        void getUSOForExportTable(size_t &ackOffset, int64_t &seqNo, std::string tableSignature);

        /**
         * Retrieve a hash code for the specified table
         */
        size_t tableHashCode(int32_t tableId);

        void updateHashinator(HashinatorType type, const char *config, int32_t *configPtr, uint32_t numTokens);

        /*
         * Execute an arbitrary task represented by the task id and serialized parameters.
         * Returns serialized representation of the results
         */
        void executeTask(TaskType taskType, const char* taskParams);

        void rebuildTableCollections();
    private:

        /*
         * Tasks dispatched by executeTask
         */
        void dispatchValidatePartitioningTask(const char *taskParams);

        void setCurrentUndoQuantum(voltdb::UndoQuantum* undoQuantum);

        std::string getClusterNameFromTable(voltdb::Table *table);
        std::string getDatabaseNameFromTable(voltdb::Table *table);

        // -------------------------------------------------
        // Initialization Functions
        // -------------------------------------------------
        bool initPlanFragment(const int64_t fragId, const std::string planNodeTree);
        bool initPlanNode(const int64_t fragId,
                          AbstractPlanNode* node,
                          TempTableLimits* limits);
        bool initCluster();
        void processCatalogDeletes(int64_t timestamp);
        void initMaterializedViews(bool addAll);
        bool updateCatalogDatabaseReference();

        bool hasSameSchema(catalog::Table *t1, voltdb::Table *t2);

        void printReport();

        /**
         * Call into the topend with information about how executing a plan fragment is going.
         */
        void reportProgessToTopend();

        /**
         * Execute a single plan fragment.
         */
        int executePlanFragment(int64_t planfragmentId,
                                int64_t inputDependencyId,
                                const NValueArray &params,
                                int64_t spHandle,
                                int64_t lastCommittedSpHandle,
                                int64_t uniqueId,
                                bool first,
                                bool last);

        /**
         * Keep a list of executors for runtime - intentionally near the top of VoltDBEngine
         */
        struct ExecutorVector {
            ExecutorVector(int64_t fragmentId,
                           int64_t logThreshold,
                           int64_t memoryLimit,
                           PlanNodeFragment *fragment) : fragId(fragmentId), planFragment(fragment)
            {
                limits.setLogThreshold(logThreshold);
                limits.setMemoryLimit(memoryLimit);
            }

            int64_t getFragId() const { return fragId; }

            const int64_t fragId;
            boost::shared_ptr<PlanNodeFragment> planFragment;
            std::vector<AbstractExecutor*> list;
            TempTableLimits limits;
        };

        /**
         * The set of plan bytes is explicitly maintained in MRU-first order,
         * while also indexed by the plans' bytes. Here lie boost-related dragons.
         */
        typedef boost::multi_index::multi_index_container<
            boost::shared_ptr<ExecutorVector>,
            boost::multi_index::indexed_by<
                boost::multi_index::sequenced<>,
                boost::multi_index::hashed_unique<
                    boost::multi_index::const_mem_fun<ExecutorVector,int64_t,&ExecutorVector::getFragId>
                >
            >
        > PlanSet;

        /**
         * Get a vector of executors for a given fragment id.
         * Get the vector from the cache if the fragment id is there.
         * If not, get a plan from the Java topend and load it up,
         * putting it in the cache and possibly bumping something else.
         */
        ExecutorVector *getExecutorVectorForFragmentId(const int64_t fragId);

        bool checkTempTableCleanup(ExecutorVector * execsForFrag);
        void cleanupExecutors(ExecutorVector * execsForFrag);

        // -------------------------------------------------
        // Data Members
        // -------------------------------------------------
        int m_currentIndexInBatch;
        int64_t m_allTuplesScanned;
        int64_t m_tuplesProcessedInBatch;
        int64_t m_tuplesProcessedInFragment;
        int64_t m_tuplesProcessedSinceReport;
        int64_t m_tupleReportThreshold;
        Table *m_lastAccessedTable;
        AbstractExecutor* m_lastAccessedExec;

        PlanSet m_plans;
        voltdb::UndoLog m_undoLog;
        voltdb::UndoQuantum *m_currentUndoQuantum;

        int64_t m_siteId;
        int32_t m_partitionId;
        int32_t m_clusterIndex;
        boost::scoped_ptr<TheHashinator> m_hashinator;
        size_t m_startOfResultBuffer;
        int64_t m_tempTableMemoryLimit;

        /*
         * Catalog delegates hashed by path.
         */
        std::map<std::string, CatalogDelegate*> m_catalogDelegates;
        std::map<std::string, CatalogDelegate*> m_delegatesByName;

        // map catalog table id to table pointers
        std::map<CatalogId, Table*> m_tables;

        // map catalog table name to table pointers
        std::map<std::string, Table*> m_tablesByName;

        /*
         * Map of catalog table ids to snapshotting tables.
         * Note that these tableIds are the ids when the snapshot
         * was initiated. The snapshot processor in Java does not
         * update tableIds when the catalog changes. The point of
         * reference, therefore, is consistently the catalog at
         * the point of snapshot initiation. It is always invalid
         * to try to map this tableId back to catalog::Table via
         * the catalog, at least w/o comparing table names.
         */
        std::map<int32_t, PersistentTable*> m_snapshottingTables;

        /*
         * Map of table signatures to exporting tables.
         */
        std::map<std::string, Table*> m_exportingTables;

        /**
         * System Catalog.
         */
        boost::shared_ptr<catalog::Catalog> m_catalog;
        catalog::Database *m_database;

        /** reused parameter container. */
        NValueArray m_staticParams;
        /** TODO : should be passed as execute() parameter..*/
        int m_usedParamcnt;

        /** buffer object for result tables. set when the result table is sent out to localsite. */
        FallbackSerializeOutput m_resultOutput;

        /** buffer object for exceptions generated by the EE **/
        ReferenceSerializeOutput m_exceptionOutput;

        /** buffer object to pass parameters to EE. */
        const char* m_parameterBuffer;
        /** size of parameter_buffer. */
        int m_parameterBufferCapacity;

        char *m_exceptionBuffer;

        int m_exceptionBufferCapacity;

        /** buffer object to receive result tables from EE. */
        char* m_reusedResultBuffer;
        /** size of reused_result_buffer. */
        int m_reusedResultCapacity;

        // arrays to hold fragment ids and dep ids from java
        // n.b. these are 8k each, should be boost shared arrays?
        int64_t m_batchFragmentIdsContainer[MAX_BATCH_COUNT];
        int64_t m_batchDepIdsContainer[MAX_BATCH_COUNT];

        /** number of plan fragments executed so far */
        int m_pfCount;

        // used for sending and recieving deps
        // set by the executeQuery / executeFrag type methods
        int m_currentInputDepId;

        /** EL subsystem on/off, pulled from catalog */
        bool m_isELEnabled;

        /** Stats manager for this execution engine **/
        voltdb::StatsAgent m_statsManager;

        /*
         * Pool for short lived strings that will not live past the return back to Java.
         */
        Pool m_stringPool;

        /*
         * When executing a plan fragment this is set to the number of result dependencies
         * that have been serialized into the m_resultOutput
         */
        int32_t m_numResultDependencies;

        LogManager m_logManager;

        char *m_templateSingleLongTable;

        const static int m_templateSingleLongTableSize
          = 4 // depid
          + 4 // table size
          + 1 // status code
          + 4 // header size
          + 2 // column count
          + 1 // column type
          + 4 + 15 // column name (length + modified_tuples)
          + 4 // tuple count
          + 4 // first row size
          + 8;// modified tuples

        Topend *m_topend;

        // For data from engine that must be shared/distributed to
        // other components. (Components MUST NOT depend on VoltDBEngine.h).
        ExecutorContext *m_executorContext;

        DefaultTupleSerializer m_tupleSerializer;

        ThreadLocalPool m_tlPool;

        int32_t m_compactionThreshold;

        // Bench
        timespec m_backendTime;
        timespec m_indexExecutorsTime;
        int64_t m_numBackendCalls;
        int64_t m_numIndexExecutorsCalls;
};

inline void VoltDBEngine::resetReusedResultOutputBuffer(const size_t headerSize) {
    m_resultOutput.initializeWithPosition(m_reusedResultBuffer, m_reusedResultCapacity, headerSize);
    m_exceptionOutput.initializeWithPosition(m_exceptionBuffer, m_exceptionBufferCapacity, headerSize);
    *reinterpret_cast<int32_t*>(m_exceptionBuffer) = voltdb::VOLT_EE_EXCEPTION_TYPE_NONE;
}

/**
 * Track total tuples accessed for this query.
 * Set up statistics for long running operations thru m_engine if total tuples accessed passes the threshold.
 */
inline int64_t VoltDBEngine::pullTuplesRemainingUntilProgressReport(AbstractExecutor* exec, Table* target_table) {
    if (target_table) {
        m_lastAccessedTable = target_table;
    }
    m_lastAccessedExec = exec;
    return m_tupleReportThreshold - m_tuplesProcessedSinceReport;
}

inline int64_t VoltDBEngine::pushTuplesProcessedForProgressMonitoring(int64_t tuplesProcessed) {
    m_tuplesProcessedSinceReport += tuplesProcessed;
    if (m_tuplesProcessedSinceReport >= m_tupleReportThreshold) {
        reportProgessToTopend();
    }
    return m_tupleReportThreshold; // size of next batch
}

inline void VoltDBEngine::pushFinalTuplesProcessedForProgressMonitoring(int64_t tuplesProcessed) {
    pushTuplesProcessedForProgressMonitoring(tuplesProcessed);
    m_lastAccessedExec = NULL;
}

} // namespace voltdb

#endif // VOLTDBENGINE_H

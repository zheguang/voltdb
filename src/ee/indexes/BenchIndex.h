#ifndef BENCHINDEX_H
#define BENCHINDEX_H

#include <cassert>
#include "common/TimeMeasure.hpp"
#include "indexes/tableindex.h"

namespace voltdb {

class BenchIndex : public TableIndex {

public:
  BenchIndex(TableIndex* wrappedIndex):
    TableIndex(wrappedIndex->m_keySchema, wrappedIndex->m_scheme)
    {
      _wrappedIndex = wrappedIndex;
      clearTime();
    }

  ~BenchIndex() {
    delete _wrappedIndex;
  }

  bool addEntry(const TableTuple *tuple) {
    startTime();
    bool result = _wrappedIndex->addEntry(tuple);
    endTime();
    return result;
  }

  bool deleteEntry(const TableTuple *tuple) {
    startTime();
    bool result = _wrappedIndex->deleteEntry(tuple);
    endTime();
    return result;
  }

  bool replaceEntryNoKeyChange(const TableTuple &destinationTuple,
                               const TableTuple &originalTuple) {
    startTime();
    bool result = _wrappedIndex->replaceEntryNoKeyChange(destinationTuple, originalTuple);
    endTime();
    return result;
  }

  bool keyUsesNonInlinedMemory() {
    startTime();
    bool result = _wrappedIndex->keyUsesNonInlinedMemory();
    endTime();
    return result;
  }

  bool exists(const TableTuple* values) {
    startTime();
    bool result = _wrappedIndex->exists(values);
    endTime();
    return result;
  }

  bool moveToKey(const TableTuple *searchKey) {
    startTime();
    bool result = _wrappedIndex->moveToKey(searchKey);
    endTime();
    return result;
  }

  void moveToKeyOrGreater(const TableTuple *searchKey) {
    startTime();
    _wrappedIndex->moveToKeyOrGreater(searchKey);
    endTime();
  }

  bool moveToGreaterThanKey(const TableTuple *searchKey) {
    startTime();
    bool result = _wrappedIndex->moveToGreaterThanKey(searchKey);
    endTime();
    return result;
  }

  void moveToLessThanKey(const TableTuple *searchKey) {
    startTime();
    _wrappedIndex->moveToLessThanKey(searchKey);
    endTime();
  }

  void moveToBeforePriorEntry() {
    startTime();
    _wrappedIndex->moveToBeforePriorEntry();
    endTime();
  }

  void moveToEnd(bool begin) {
    startTime();
    _wrappedIndex->moveToEnd(begin);
    endTime();
  }

  TableTuple nextValue() {
    startTime();
    TableTuple result = _wrappedIndex->nextValue();
    endTime();
    return result;
  }

  TableTuple nextValueAtKey() {
    startTime();
    TableTuple result = _wrappedIndex->nextValueAtKey();
    endTime();
    return result;
  }

  bool advanceToNextKey() {
    startTime();
    bool result = _wrappedIndex->advanceToNextKey();
    endTime();
    return result;
  }

  TableTuple uniqueMatchingTuple(const TableTuple &searchTuple) {
    startTime();
    TableTuple result = _wrappedIndex->uniqueMatchingTuple(searchTuple);
    endTime();
    return result;
  }

  bool checkForIndexChange(const TableTuple *lhs, const TableTuple *rhs) {
    startTime();
    bool result = _wrappedIndex->checkForIndexChange(lhs, rhs);
    endTime();
    return result;
  }

  bool hasKey(const TableTuple *searchKey) {
    startTime();
    bool result = _wrappedIndex->hasKey(searchKey);
    endTime();
    return result;
  }

  int64_t getCounterGET(const TableTuple *searchKey, bool isUpper) {
    startTime();
    int64_t result = _wrappedIndex->getCounterGET(searchKey, isUpper);
    endTime();
    return result;
  }

  int64_t getCounterLET(const TableTuple *searchKey, bool isUpper) {
    startTime();
    int64_t result = _wrappedIndex->getCounterLET(searchKey, isUpper);
    endTime();
    return result;
  }

  size_t getSize() const {
    return _wrappedIndex->getSize();
  }

  int64_t getMemoryEstimate() const {
    return getMemoryEstimate();
  }

  std::string debug() const {
    return _wrappedIndex->debug();
  }
  
  std::string getTypeName() const {
    return _wrappedIndex->getTypeName();
  }

  void ensureCapacity(uint32_t capacity) {
    startTime();
    _wrappedIndex->ensureCapacity(capacity);
    endTime();
  }

  void printReport() {
    _wrappedIndex->printReport();
  }

  bool equals(const TableIndex *other) const {
    return _wrappedIndex->equals(other);
  }

  voltdb::IndexStats* getIndexStats() {
    return _wrappedIndex->getIndexStats();
  }
  
  timespec getTime() const {
    assert (_numStartTime == 0);
    return _time;
  }

  void clearTime() {
    _numStartTime = 0;
    _startTime.tv_sec = 0;
    _startTime.tv_nsec = 0;
    _time.tv_sec = 0;
    _time.tv_nsec = 0;
  }

protected:
  void startTime() {
    assert (_numStartTime >= 0);
    if (_numStartTime == 0) {
      clock_gettime(CLOCK_REALTIME, &_startTime);
    }
    _numStartTime++;
  }

  void endTime() {
    assert (_numStartTime > 0);
    if (_numStartTime == 1) {
      timespec endTime;
      clock_gettime(CLOCK_REALTIME, &endTime);
      _time = TimeMeasure::sum(_time, TimeMeasure::diff(_startTime, endTime));
    }
    _numStartTime--;
  }

  virtual TableIndex *cloneEmptyNonCountingTreeIndex() const {
    return _wrappedIndex->cloneEmptyNonCountingTreeIndex();
  }

private:
  TableIndex* _wrappedIndex;

  int _numStartTime;
  timespec _startTime;
  timespec _time;
};

}

#endif

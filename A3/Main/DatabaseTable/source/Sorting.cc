
#ifndef SORT_C
#define SORT_C

#include "Sorting.h"
#include "MyDB_PageReaderWriter.h"
#include "MyDB_TableReaderWriter.h"
#include "MyDB_TableRecIterator.h"
#include "MyDB_TableRecIteratorAlt.h"

using namespace std;

#include <algorithm>
#include <queue>
#include <vector>

#include <algorithm>
#include <queue>
#include <vector>

// Comparator struct for the priority queue in mergeIntoFile
struct MergeRecord {
  MyDB_RecordIteratorAltPtr iterator;
};

class IteratorComparator {
  function<bool()> comparator;
  MyDB_RecordPtr lhs;
  MyDB_RecordPtr rhs;

public:
  IteratorComparator(function<bool()> comp, MyDB_RecordPtr l, MyDB_RecordPtr r)
      : comparator(comp), lhs(l), rhs(r) {}

  bool operator()(const MyDB_RecordIteratorAltPtr &a,
                  const MyDB_RecordIteratorAltPtr &b) {
    // Load 'a' into lhs
    a->getCurrent(lhs);
    // Load 'b' into rhs
    b->getCurrent(rhs);

    // Check if lhs < rhs
    if (comparator()) {
      return false; // a < b, so for max heap we return false to keep a at top?
                    // No. std::priority_queue is max heap. Top is largest. We
                    // want Top to be smallest. So if a < b, we want a to be
                    // "greater" in the heap sense? Or we want `operator<` to
                    // return true if a "comes after" b? Let's assume we want
                    // std::greater behavior. If compare(a,b) is true (a < b),
                    // then a should be popped BEFORE b. In a max heap, the
                    // LARGEST pops first. So we want a < b to imply a > b in
                    // the heap's comparison? If heap uses Comp(x,y), it puts x
                    // above y if Comp(x,y) is false? Standard: pop() returns
                    // element X such that Comp(X, Y) is false for all Y. i.e. X
                    // is the "largest". We want to pop the "smallest" (a < b).
                    // So we want Comp(a, b) to be true if a > b.
                    // Our comparator() returns true if lhs < rhs.
                    // So if lhs < rhs (a < b), we return false.
                    // If rhs < lhs (b < a), we return true.
      return false;
    }

    // If not a < b, check b < a

    // Load b into lhs
    b->getCurrent(lhs);
    // Load a into rhs
    a->getCurrent(rhs);

    if (comparator()) {
      // b < a. We return true.
      return true;
    }

    return false; // Equal
  }
};

void mergeIntoFile(MyDB_TableReaderWriter &sortIntoMe,
                   vector<MyDB_RecordIteratorAltPtr> &mergeUs,
                   function<bool()> comparator, MyDB_RecordPtr lhs,
                   MyDB_RecordPtr rhs) {

  std::priority_queue<MyDB_RecordIteratorAltPtr,
                      std::vector<MyDB_RecordIteratorAltPtr>,
                      IteratorComparator>
      pQueue(IteratorComparator(comparator, lhs, rhs));

  for (auto &iter : mergeUs) {
    if (iter->advance()) {
      pQueue.push(iter);
    }
  }

  while (!pQueue.empty()) {
    MyDB_RecordIteratorAltPtr smallestIter = pQueue.top();
    pQueue.pop();

    smallestIter->getCurrent(lhs);
    sortIntoMe.append(lhs);

    if (smallestIter->advance()) {
      pQueue.push(smallestIter);
    }
  }
}

vector<MyDB_PageReaderWriter>
mergeIntoList(MyDB_BufferManagerPtr parent, MyDB_RecordIteratorAltPtr leftIter,
              MyDB_RecordIteratorAltPtr rightIter, function<bool()> comparator,
              MyDB_RecordPtr lhs, MyDB_RecordPtr rhs) {

  vector<MyDB_PageReaderWriter> resultPages;
  // Use proper constructor for anonymous page:
  // MyDB_PageReaderWriter(MyDB_BufferManager &)
  MyDB_PageReaderWriter currentOutputPage(*parent);
  currentOutputPage.clear();
  resultPages.push_back(currentOutputPage);

  bool leftHasMore = leftIter->advance();
  bool rightHasMore = rightIter->advance();

  while (leftHasMore && rightHasMore) {
    leftIter->getCurrent(lhs);
    rightIter->getCurrent(rhs);

    if (comparator()) { // lhs < rhs
      if (!resultPages.back().append(lhs)) {
        MyDB_PageReaderWriter newPage(*parent);
        newPage.clear();
        resultPages.push_back(newPage);
        resultPages.back().append(lhs);
      }
      leftHasMore = leftIter->advance();
    } else { // rhs <= lhs
      if (!resultPages.back().append(rhs)) {
        MyDB_PageReaderWriter newPage(*parent);
        newPage.clear();
        resultPages.push_back(newPage);
        resultPages.back().append(rhs);
      }
      rightHasMore = rightIter->advance();
    }
  }

  while (leftHasMore) {
    leftIter->getCurrent(lhs);
    if (!resultPages.back().append(lhs)) {
      MyDB_PageReaderWriter newPage(*parent);
      newPage.clear();
      resultPages.push_back(newPage);
      resultPages.back().append(lhs);
    }
    leftHasMore = leftIter->advance();
  }

  while (rightHasMore) {
    rightIter->getCurrent(rhs);
    if (!resultPages.back().append(rhs)) {
      MyDB_PageReaderWriter newPage(*parent);
      newPage.clear();
      resultPages.push_back(newPage);
      resultPages.back().append(rhs);
    }
    rightHasMore = rightIter->advance();
  }

  return resultPages;
}

void sort(int runSize, MyDB_TableReaderWriter &sortMe,
          MyDB_TableReaderWriter &sortIntoMe, function<bool()> comparator,
          MyDB_RecordPtr lhs, MyDB_RecordPtr rhs) {

  MyDB_BufferManagerPtr myMgr = sortMe.getBufferMgr();
  vector<MyDB_RecordIteratorAltPtr> mergePhaseIterators;
  int numPages = sortMe.getNumPages();

  for (int i = 0; i < numPages; i += runSize) {
    vector<MyDB_PageReaderWriter> pagesInRun;

    for (int j = 0; j < runSize && (i + j) < numPages; j++) {
      pagesInRun.push_back(sortMe[i + j]);
      pagesInRun.back().sortInPlace(comparator, lhs, rhs);
    }

    vector<vector<MyDB_PageReaderWriter>> subRuns;
    for (auto &p : pagesInRun) {
      vector<MyDB_PageReaderWriter> singlePageRun;
      singlePageRun.push_back(p);
      subRuns.push_back(singlePageRun);
    }

    while (subRuns.size() > 1) {
      vector<vector<MyDB_PageReaderWriter>> nextSubRuns;
      for (auto it = subRuns.begin(); it != subRuns.end();) {
        vector<MyDB_PageReaderWriter> &run1 = *it;
        it++;
        if (it != subRuns.end()) {
          vector<MyDB_PageReaderWriter> &run2 = *it;
          it++;

          MyDB_RecordIteratorAltPtr iter1 = getIteratorAlt(run1);
          MyDB_RecordIteratorAltPtr iter2 = getIteratorAlt(run2);
          nextSubRuns.push_back(
              mergeIntoList(myMgr, iter1, iter2, comparator, lhs, rhs));
        } else {
          nextSubRuns.push_back(run1);
        }
      }
      subRuns = nextSubRuns;
    }

    if (!subRuns.empty()) {
      mergePhaseIterators.push_back(getIteratorAlt(subRuns[0]));
    }
  }

  mergeIntoFile(sortIntoMe, mergePhaseIterators, comparator, lhs, rhs);
}

#endif

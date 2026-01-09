
#include "MyDB_AttType.h"
#include "MyDB_AttVal.h"
#include "MyDB_BufferManager.h"
#include "MyDB_Catalog.h"
#include "MyDB_Record.h"
#include "MyDB_Schema.h"
#include "MyDB_Table.h"
#include "MyDB_TableReaderWriter.h"
#include "MyDB_TableRecIterator.h"
#include "QUnit.h"
#include "Sorting.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace std;

int main() {

  QUnit::UnitTest qunit(std::cerr, QUnit::verbose);

  // Test 1: Sort a collection of integers
  {
    cout << "Running SortIntegers test..." << endl;

    // Setup catalog and schema
    MyDB_CatalogPtr myCatalog = make_shared<MyDB_Catalog>("catFile");

    // Define a schema with an integer attribute "val"
    MyDB_SchemaPtr mySchema = make_shared<MyDB_Schema>();
    mySchema->appendAtt(make_pair("val", make_shared<MyDB_IntAttType>()));

    // Create table for input
    MyDB_TablePtr myTable =
        make_shared<MyDB_Table>("inputTable", "inputTable_storage", mySchema);
    MyDB_BufferManagerPtr myMgr =
        make_shared<MyDB_BufferManager>(1024 * 128, 128, "tempFile");
    MyDB_TableReaderWriterPtr inputTableRW =
        make_shared<MyDB_TableReaderWriter>(myTable, myMgr);

    // Populate with random integers
    MyDB_RecordPtr rec = inputTableRW->getEmptyRecord();
    for (int i = 0; i < 1000; i++) {
      int val = rand() % 10000;
      rec->getAtt(0)->fromInt(val);
      inputTableRW->append(rec);
    }

    // Create table for output
    MyDB_TablePtr outputTable =
        make_shared<MyDB_Table>("outputTable", "outputTable_storage", mySchema);
    MyDB_TableReaderWriterPtr outputTableRW =
        make_shared<MyDB_TableReaderWriter>(outputTable, myMgr);

    // Define comparator: returns true if lhs.val < rhs.val
    MyDB_RecordPtr lhs = inputTableRW->getEmptyRecord();
    MyDB_RecordPtr rhs = inputTableRW->getEmptyRecord();

    // "buildRecordComparator" builds a function that returns true if
    // computation(lhs) < computation(rhs). So passing "[val]" means it compares
    // lhs["val"] < rhs["val"].
    function<bool()> myComparator = buildRecordComparator(lhs, rhs, "[val]");

    // Run Sort
    // Run size = 4
    sort(4, *inputTableRW, *outputTableRW, myComparator, lhs, rhs);

    // Verify
    MyDB_RecordPtr checkRec = outputTableRW->getEmptyRecord();
    MyDB_RecordIteratorPtr iter = outputTableRW->getIterator(checkRec);

    int lastVal = -1;
    int count = 0;
    int totalExpected = 1000; // Matches input
    bool orderingCorrect = true;

    while (iter->hasNext()) {
      iter->getNext();
      int curVal = checkRec->getAtt(0)->toInt();

      // Check ordering
      if (lastVal != -1 && curVal < lastVal) {
        orderingCorrect = false;
        cout << "Ordering Fail at index " << count << ": " << curVal << " < "
             << lastVal << endl;
      }
      lastVal = curVal;
      count++;
    }

    QUNIT_IS_EQUAL(count, totalExpected);
    QUNIT_IS_TRUE(orderingCorrect);
  }

  return qunit.errors();
}

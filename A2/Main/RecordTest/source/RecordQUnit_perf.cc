
#ifndef RECORD_TEST_PERF_H
#define RECORD_TEST_PERF_H

#include "MyDB_AttType.h"  
#include "MyDB_BufferManager.h"
#include "MyDB_Catalog.h"  
#include "MyDB_Page.h"
#include "MyDB_PageReaderWriter.h"
#include "MyDB_Record.h"
#include "MyDB_Table.h"
#include "MyDB_TableReaderWriter.h"
#include "MyDB_Schema.h"
#include "QUnit.h"
#include <cstring>
#include <iostream>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <chrono>

#define FALLTHROUGH_INTENDED do {} while (0)

using namespace std;

// Helper to clean up previous runs
void cleanup() {
    unlink("perfCatFile");
    unlink("perfTempFile");
    unlink("perfTable.bin");
}

int main(int argc, char *argv[]) {
	int start = 1;
	if (argc > 1 && argv[1][0] >= '0' && argv[1][0] <= '9') {
		start = argv[1][0] - '0';
	}
	cout << "start from test " << start << endl << flush;

	QUnit::UnitTest qunit(cerr, QUnit::normal);

	switch (start) {
	case 1:
	{
		// Test 1: Write Performance (Bulk Append)
		// Appending 50,000 records.
		cout << "TEST 1: Bulk Append Performance..." << endl << flush;
        cleanup();

		// create a catalog
		MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog>("perfCatFile");

		// now make a schema
		MyDB_SchemaPtr mySchema = make_shared <MyDB_Schema>();
		mySchema->appendAtt(make_pair("suppkey", make_shared <MyDB_IntAttType>()));
		mySchema->appendAtt(make_pair("name", make_shared <MyDB_StringAttType>()));
		mySchema->appendAtt(make_pair("address", make_shared <MyDB_StringAttType>()));
		mySchema->appendAtt(make_pair("nationkey", make_shared <MyDB_IntAttType>()));
		mySchema->appendAtt(make_pair("phone", make_shared <MyDB_StringAttType>()));
		mySchema->appendAtt(make_pair("acctbal", make_shared <MyDB_DoubleAttType>()));
		mySchema->appendAtt(make_pair("comment", make_shared <MyDB_StringAttType>()));

		// use the schema to create a table
		MyDB_TablePtr myTable = make_shared <MyDB_Table>("perfTable", "perfTable.bin", mySchema);
        
        // Small buffer (64 pages) to force I/O
		MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager>(1024, 64, "perfTempFile");
		MyDB_TableReaderWriter perfTable(myTable, myMgr);

        // Prepare a record
		MyDB_RecordPtr temp = perfTable.getEmptyRecord();
        string s = "10001|Supplier#000010001|00000000|999|12-345-678-9012|1234.56|the special record|";
        temp->fromString(s);

        int numRecords = 50000;
        cout << "Appending " << numRecords << " records..." << flush;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numRecords; i++) {
            perfTable.append(temp);
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        cout << " Done." << endl;
        cout << "Time taken: " << duration << " ms" << endl;
        cout << "Throughput: " << (double)numRecords / (duration / 1000.0) << " records/sec" << endl;

        // Roughly check if it was fast enough? 
        // 50k records should be < 5 seconds even with I/O overhead on most SSDs
        if (duration < 10000) cout << "CORRECT (Performance within limits)" << endl << flush;
		else cout << "***FAIL*** (Too slow)" << endl << flush;
		
        // Verify count
        MyDB_RecordIteratorPtr myIter = perfTable.getIterator(temp);
        int counter = 0;
        while (myIter->hasNext()) {
            myIter->getNext();
            counter++;
        }
        
        QUNIT_IS_EQUAL(counter, numRecords);

        // Update catalog with new table state (lastPage)
        myTable->putInCatalog(myCatalog);
	}
	FALLTHROUGH_INTENDED;
	case 2:
	{
		// Test 2: Scan Performance
		// Scanning the table created in Test 1
		cout << "TEST 2: Full Table Scan Performance..." << endl << flush;
        
        // Re-open to clear buffer cache effect (mostly)
        MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog>("perfCatFile");
        map <string, MyDB_TablePtr> allTables = MyDB_Table::getAllTables(myCatalog);
        // Larger buffer this time to test memory speed? Or keep small to test I/O?
        // Let's keep small to be consistent.
		MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager>(1024, 64, "perfTempFile");
		MyDB_TableReaderWriter perfTable(allTables["perfTable"], myMgr);

		MyDB_RecordPtr temp = perfTable.getEmptyRecord();
        MyDB_RecordIteratorPtr myIter = perfTable.getIterator(temp);

        int numRecords = 50000;
        int counter = 0;

        cout << "Scanning " << numRecords << " records..." << flush;
        auto start_time = std::chrono::high_resolution_clock::now();

        while (myIter->hasNext()) {
            myIter->getNext();
            counter++;
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        cout << " Done." << endl;
        cout << "Scanned " << counter << " records." << endl;
        cout << "Time taken: " << duration << " ms" << endl;
        cout << "Throughput: " << (double)counter / (duration / 1000.0) << " records/sec" << endl;

		if (counter == numRecords) cout << "CORRECT" << endl << flush;
		else cout << "***FAIL***" << endl << flush;
		QUNIT_IS_EQUAL(counter, numRecords);
	}
	FALLTHROUGH_INTENDED;
    case 3:
    {
        // Test 3: Alternating Scans (Stress Test)
        // Two iterators open at the same time, verifying they don't interfere and pages are swapped correctly.
        cout << "TEST 3: Alternating Scans Performance..." << endl << flush;

        MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog>("perfCatFile");
        map <string, MyDB_TablePtr> allTables = MyDB_Table::getAllTables(myCatalog);
		MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager>(1024, 16, "perfTempFile"); // Tiny buffer!
		MyDB_TableReaderWriter perfTable(allTables["perfTable"], myMgr);

        MyDB_RecordPtr temp = perfTable.getEmptyRecord();
        MyDB_RecordIteratorPtr iterA = perfTable.getIterator(temp);
        MyDB_RecordIteratorPtr iterB = perfTable.getIterator(temp);

        int countA = 0;
        int countB = 0;
        
        // Advance B by 100 records first
        for(int k=0; k<100; k++) if(iterB->hasNext()) iterB->getNext();

        cout << "Running parallel iterators..." << flush;
        auto start_time = std::chrono::high_resolution_clock::now();

        // Iterate both until done
        bool active = true;
        while(active) {
            active = false;
            // Advance A by 10
            for(int k=0; k<10; k++) {
                if(iterA->hasNext()) {
                    iterA->getNext();
                    countA++;
                    active = true;
                }
            }
            // Advance B by 10
            for(int k=0; k<10; k++) {
                if(iterB->hasNext()) {
                    iterB->getNext();
                    countB++;
                    active = true;
                }
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        cout << " Done." << endl;
        cout << "Time taken: " << duration << " ms" << endl;
        cout << "CountA: " << countA << ", CountB: " << countB << endl;

        // Since B started 100 ahead, it effectively skipped 100 in the "counting loop" logic? 
        // No, I called getNext() on B 100 times before the loop, so its internal counter is technically -100 relative to full, but we didn't count those?
        // Ah, `countB` only counts loop iterations. The first 100 were skipped.
        // Wait, iterB advanced 100. Then inside loop we continue.
        // Total records = 50000.
        // A should read 50000.
        // B should read 50000 - 100 = 49900.
        
        // Verification logic
        if (countA == 50000 && countB == 49900) cout << "CORRECT" << endl << flush;
		else cout << "***FAIL*** (Counts mismatch)" << endl << flush;
        
        QUNIT_IS_EQUAL(countA, 50000);
        QUNIT_IS_EQUAL(countB, 49900);
    }
    FALLTHROUGH_INTENDED;
	default:
		break;
	}
    
    // Cleanup
    cleanup();
}

#endif

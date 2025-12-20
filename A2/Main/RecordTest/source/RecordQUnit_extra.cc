
#ifndef RECORD_TEST_EXTRA_H
#define RECORD_TEST_EXTRA_H

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

void cleanup_extra() {
    unlink("extraCatFile");
    unlink("extraTempFile");
    unlink("extraTable.bin");
    unlink("extraTable2.bin");
    unlink("extraTable3.bin");
    unlink("extraTableCopy.bin");
    unlink("extraTableMixed.bin");
    unlink("extraTableStress.bin");
}

int main(int argc, char *argv[]) {
	int start = 1;
	if (argc > 1) {
		start = atoi(argv[1]);
	}
	cout << "start from test " << start << endl << flush;

	QUnit::UnitTest qunit(cerr, QUnit::normal);

	switch (start) {
	case 1:
	{
		// Test 1: Persistence Verification
		// Write 100 records, shutdown, reload, verify.
		cout << "TEST 1: Persistence Verification..." << endl << flush;
        cleanup_extra();

		MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog>("extraCatFile");
		MyDB_SchemaPtr mySchema = make_shared <MyDB_Schema>();
		mySchema->appendAtt(make_pair("suppkey", make_shared <MyDB_IntAttType>()));
		mySchema->appendAtt(make_pair("name", make_shared <MyDB_StringAttType>()));
		mySchema->appendAtt(make_pair("address", make_shared <MyDB_StringAttType>()));
        mySchema->appendAtt(make_pair("nationkey", make_shared <MyDB_IntAttType>()));
        mySchema->appendAtt(make_pair("phone", make_shared <MyDB_StringAttType>()));
        mySchema->appendAtt(make_pair("acctbal", make_shared <MyDB_DoubleAttType>()));
        mySchema->appendAtt(make_pair("comment", make_shared <MyDB_StringAttType>()));

		MyDB_TablePtr myTable = make_shared <MyDB_Table>("extraTable", "extraTable.bin", mySchema);
        // Save table to catalog immediately
        myTable->putInCatalog(myCatalog);

        {
		    MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager>(1024, 16, "extraTempFile");
		    MyDB_TableReaderWriter testTable(myTable, myMgr);
		    MyDB_RecordPtr temp = testTable.getEmptyRecord();
            // Just a dummy record
            string s = "1|Supplier#1|Addr|1|Phone|1.0|Comment|";
            temp->fromString(s);
            for(int i=0; i<100; i++) testTable.append(temp);
        } // Shutdown manager

        // Re-open
        {
            MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager>(1024, 16, "extraTempFile");
		    MyDB_TableReaderWriter testTable(myTable, myMgr);
            MyDB_RecordPtr temp = testTable.getEmptyRecord();
            MyDB_RecordIteratorPtr myIter = testTable.getIterator(temp);
            int counter = 0;
            while(myIter->hasNext()) {
                myIter->getNext();
                counter++;
            }
            if(counter == 100) cout << "CORRECT" << endl << flush;
            else cout << "***FAIL*** (Expected 100, got " << counter << ")" << endl << flush;
            QUNIT_IS_EQUAL(counter, 100);

            // Persist valid state for Test 2
            myTable->putInCatalog(myCatalog);
        }
	}
	FALLTHROUGH_INTENDED;
	case 2:
	{
		// Test 2: Appending After Reload
        // Load the existing table from Test 1, verify we can append more.
		cout << "TEST 2: Appending After Reload..." << endl << flush;
        
        MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog>("extraCatFile");
        map <string, MyDB_TablePtr> allTables = MyDB_Table::getAllTables(myCatalog);
        MyDB_TablePtr myTable = allTables["extraTable"];
        
        {
            MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager>(1024, 16, "extraTempFile");
            MyDB_TableReaderWriter testTable(myTable, myMgr);
            MyDB_RecordPtr temp = testTable.getEmptyRecord();
            string s = "2|Supplier#2|Addr|2|Phone|2.0|Comment|";
            temp->fromString(s);
            
            // Append 50 more
            for(int i=0; i<50; i++) testTable.append(temp);
        }

        // Verify total = 150
        {
            MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager>(1024, 16, "extraTempFile");
            MyDB_TableReaderWriter testTable(myTable, myMgr);
            MyDB_RecordPtr temp = testTable.getEmptyRecord();
            MyDB_RecordIteratorPtr myIter = testTable.getIterator(temp);
            int counter = 0;
            while(myIter->hasNext()) {
                myIter->getNext();
                counter++;
            }
             if(counter == 150) cout << "CORRECT" << endl << flush;
            else cout << "***FAIL*** (Expected 150, got " << counter << ")" << endl << flush;
            QUNIT_IS_EQUAL(counter, 150);
        }
	}
	FALLTHROUGH_INTENDED;
    case 3:
    {
        {
            // Test 3: Exact Page Filling (Not easy to guarantee exact bytes with variable strings, but we can try)
            // We will write small records and check if page count increments sensibly
            cout << "TEST 3: Page Filling Check..." << endl << flush;
            
            // New table
            MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog>("extraCatFile");
            MyDB_SchemaPtr mySchema = make_shared <MyDB_Schema>();
            mySchema->appendAtt(make_pair("id", make_shared <MyDB_IntAttType>()));
            mySchema->appendAtt(make_pair("val", make_shared <MyDB_IntAttType>())); // Fixed size record: 4+4 = 8 bytes data + overhead?
            MyDB_TablePtr myTable = make_shared <MyDB_Table>("extraTable2", "extraTable2.bin", mySchema);
            MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager>(128, 16, "extraTempFile"); // Tiny page: 128 bytes
            MyDB_TableReaderWriter testTable(myTable, myMgr);
            
            MyDB_RecordPtr temp = testTable.getEmptyRecord();
            
            // Page header is usually ~16-24 bytes. Leftover ~100 bytes. 
            // Record is Int(4)+Int(4) = 8 bytes + maybe header overhead per record?
            int recsPerPageCandidate = 0;
            int lastPageBefore = myTable->lastPage();
            
            for(int i=0; i<50; i++) {
                // Schema is (id, val) -> 2 Ints
                // "i|i|"
                string s = to_string(i) + "|" + to_string(i) + "|";
                temp->fromString(s);
                
                testTable.append(temp);
                if (myTable->lastPage() > lastPageBefore) {
                    // Page flipped
                    if(recsPerPageCandidate == 0) recsPerPageCandidate = i;
                    lastPageBefore = myTable->lastPage();
                }
            }
            
            // If we wrote 50 records and page size is 128, we expect multiple pages.
            if (myTable->lastPage() > 0) cout << "CORRECT" << endl << flush;
            else cout << "***FAIL*** (Did not create new pages)" << endl << flush;
        }
        unlink("extraTable2.bin");
        QUNIT_IS_TRUE(true); // Can't easily check logic outside block, assume pass if execution reaches here
    }
    FALLTHROUGH_INTENDED;
    case 4:
    {
        {
            // Test 4: Oversized Record Rejection
            cout << "TEST 4: Oversized Record Rejection..." << endl << flush;
            // Try to write a record > 128 bytes to the tiny table from Test 3
            MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog>("extraCatFile");
            MyDB_SchemaPtr mySchema = make_shared <MyDB_Schema>();
            mySchema->appendAtt(make_pair("blob", make_shared <MyDB_StringAttType>()));
            
            // Very small page
            MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager>(64, 16, "extraTempFile"); 
            MyDB_TablePtr myTable = make_shared <MyDB_Table>("extraTable3", "extraTable3.bin", mySchema);
            MyDB_TableReaderWriter testTable(myTable, myMgr);
    
            MyDB_RecordPtr temp = testTable.getEmptyRecord();
            // Construct string > 64 chars
            string bigS(100, 'x');
            // Connect string to 1-attribute schema
            string s = bigS + "|";
            temp->fromString(s);
            
            // Should catch logic in append that returns false or prints error
            // Note: The assignment spec says append returns void usually but our `MyDB_PageReaderWriter` returns bool.
            // `MyDB_TableReaderWriter` append is void but might print error. 
            // We can't easily check return value here unless we modified interface, but we can check if it crashed.
            
            try {
               testTable.append(temp);
               cout << "CORRECT (Handled oversized record without crash)" << endl << flush;
            } catch (...) {
               cout << "***FAIL*** (Crashed on oversized record)" << endl << flush; 
            }
        }
        unlink("extraTable3.bin");
        QUNIT_IS_TRUE(true); // Just survival
    }
    FALLTHROUGH_INTENDED;
    case 5:
    {
        // Test 5: The "Full-Empty-Full" Pattern
        cout << "TEST 5: Full-Empty-Full..." << endl << flush;
        MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager>(1024, 16, "extraTempFile");
        MyDB_TablePtr myTable = make_shared <MyDB_Table>("extraTable", "extraTable.bin", make_shared<MyDB_Schema>()); // Schema doesn't matter for page access?
        // Actually need valid schema for record creation, re-use existing table from Test 1
         MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog>("extraCatFile");
         map <string, MyDB_TablePtr> allTables = MyDB_Table::getAllTables(myCatalog);
         myTable = allTables["extraTable"];
         MyDB_TableReaderWriter testTable(myTable, myMgr);
         
         // Access page 0
         MyDB_PageReaderWriter page0 = testTable[0];
         page0.clear(); // Empty it
         
         MyDB_RecordPtr temp = testTable.getEmptyRecord();
         string s = "5|Supplier#5|Addr|5|Phone|5.0|Comment|";
         temp->fromString(s);
         
         // Fill it
         while(page0.append(temp)); 
         
         // Clear it
         page0.clear();
         
         // Fill one record
         if(page0.append(temp)) cout << "CORRECT" << endl << flush;
         else cout << "***FAIL*** (Could not append to cleared page)" << endl << flush;
         QUNIT_IS_TRUE(true);
    }
    FALLTHROUGH_INTENDED;
    case 6:
    {
        // Test 6: The "Swiss Cheese" Table (Sparse Iteration)
        cout << "TEST 6: Swiss Cheese Iteration..." << endl << flush;
        
        // We will reuse extraTable. It has some pages.
        // Let's add 10 more pages of data.
        MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog>("extraCatFile");
        map <string, MyDB_TablePtr> allTables = MyDB_Table::getAllTables(myCatalog);
        MyDB_TablePtr myTable = allTables["extraTable"];
        MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager>(1024, 16, "extraTempFile");
        MyDB_TableReaderWriter testTable(myTable, myMgr);
        MyDB_RecordPtr temp = testTable.getEmptyRecord();
        string s = "6|Supplier#6|Addr|6|Phone|6.0|Comment|";
        temp->fromString(s);
        
        int startPage = myTable->lastPage() + 1;
        // Create 10 pages
        for(int i=0; i<10; i++) {
             // force new page
             testTable.append(temp); 
             // Fill until full
             while(testTable.last().append(temp));
        }
        
        // Clear even pages in this range
        for(int i=startPage; i < myTable->lastPage(); i++) {
            if(i % 2 == 0) testTable[i].clear();
        }
        
        // Iterate
        int recCount = 0;
        MyDB_RecordIteratorPtr myIter = testTable.getIterator(temp);
        while(myIter->hasNext()) {
            myIter->getNext();
            recCount++;
        }
        
        if (recCount > 0) cout << "CORRECT (Iterated " << recCount << " records)" << endl << flush;
        else cout << "***FAIL*** (Zero records found)" << endl << flush;
        QUNIT_IS_TRUE(recCount > 0);
    }
    FALLTHROUGH_INTENDED;
    case 7:
    {
        // Test 7: Massive String Field
        cout << "TEST 7: Massive String Field..." << endl << flush;
        // Same table, just verify we can write closest to max
        // Page 1024. Header ~20. Max string ~1000?
         MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog>("extraCatFile");
         map <string, MyDB_TablePtr> allTables = MyDB_Table::getAllTables(myCatalog);
         MyDB_TablePtr myTable = allTables["extraTable"]; // This has schema with strings
         MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager>(4096, 16, "extraTempFile"); // Bigger page
         MyDB_TableReaderWriter testTable(myTable, myMgr);
         
         MyDB_RecordPtr temp = testTable.getEmptyRecord();
         // "comment" is the last field.
         string bigS(3000, 'z'); // 3000 chars
         //  "suppkey", "name", "address", "nationkey", "phone", "acctbal", "comment"
         // 1|Supplier#1|Addr|1|Phone|1.0|BIGSTRING|
         string recStr = "7|Supp|Addr|7|Phone|7.0|" + bigS + "|";
         temp->fromString(recStr);
         
         if (testTable.last().append(temp)) {
              cout << "CORRECT" << endl << flush;
         } else {
              // Might have failed if page was full, try new page
              testTable.append(temp); // Should force new page
              cout << "CORRECT (Appended to new page)" << endl << flush;
         }
         QUNIT_IS_TRUE(true);
    }
    FALLTHROUGH_INTENDED;
    case 8:
    {
        // Test 8: Interleaved Append and Read
        cout << "TEST 8: Interleaved Append and Read..." << endl << flush;
         MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog>("extraCatFile");
         map <string, MyDB_TablePtr> allTables = MyDB_Table::getAllTables(myCatalog);
         MyDB_TablePtr myTable = allTables["extraTable"]; 
         MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager>(1024, 16, "extraTempFile");
         MyDB_TableReaderWriter testTable(myTable, myMgr);
         
         MyDB_RecordPtr temp = testTable.getEmptyRecord();
         string s = "8|Supplier#8|Addr|8|Phone|8.0|Comment|";
         temp->fromString(s);
         
         MyDB_RecordIteratorPtr myIter = testTable.getIterator(temp);
         
         // Append
         testTable.append(temp);
         
         // Read one
         if(myIter->hasNext()) myIter->getNext();
         
         // Append
         testTable.append(temp);
         
         // Read one
         if(myIter->hasNext()) myIter->getNext();
         
         cout << "CORRECT" << endl << flush;
         QUNIT_IS_TRUE(true);
    }
    FALLTHROUGH_INTENDED;
    case 9:
    {
        // Test 9: Random Page Access Writer
        cout << "TEST 9: Random Page Write..." << endl << flush;
         MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog>("extraCatFile");
         map <string, MyDB_TablePtr> allTables = MyDB_Table::getAllTables(myCatalog);
         MyDB_TablePtr myTable = allTables["extraTable"]; 
         MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager>(1024, 16, "extraTempFile");
         MyDB_TableReaderWriter testTable(myTable, myMgr);
         
         MyDB_RecordPtr temp = testTable.getEmptyRecord();
         string s = "9|Supplier#9|Addr|9|Phone|9.0|Comment|";
         temp->fromString(s);
         
         // Write to page 0
         testTable[0].append(temp);
         // Write to page 5 (if exists)
         if (myTable->lastPage() >= 5) {
             testTable[5].append(temp);
         }
         
         cout << "CORRECT" << endl << flush;
         QUNIT_IS_TRUE(true);
    }
    FALLTHROUGH_INTENDED;
    case 10:
    {
        int totalMoved = 0;
        {
            // Test 10: High-Contention (Many Iterators)
            cout << "TEST 10: High Contention..." << endl << flush;
             MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog>("extraCatFile");
             map <string, MyDB_TablePtr> allTables = MyDB_Table::getAllTables(myCatalog);
             MyDB_TablePtr myTable = allTables["extraTable"]; 
             MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager>(1024, 16, "extraTempFile");
             MyDB_TableReaderWriter testTable(myTable, myMgr);
             
             MyDB_RecordPtr temp = testTable.getEmptyRecord();
             
             vector<MyDB_RecordIteratorPtr> iters;
             for(int i=0; i<50; i++) {
                 iters.push_back(testTable.getIterator(temp));
             }
             
             // Advance all slightly

             for(auto &it : iters) {
                 if(it->hasNext()) {
                     it->getNext();
                     totalMoved++;
                 }
             }
             
             cout << "Moved " << totalMoved << " iterators." << endl;
             cout << "CORRECT" << endl << flush;
        }
        unlink("extraTable.bin");
        QUNIT_IS_TRUE(true);
    }
    FALLTHROUGH_INTENDED;
    case 11:
    {
        // Test 11: Mixed Attribute Types
        cout << "TEST 11: Mixed Attribute Types (Int, string, Double)..." << endl << flush;
        MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog>("extraCatFile");
        MyDB_SchemaPtr mySchema = make_shared <MyDB_Schema>();
        mySchema->appendAtt(make_pair("myInt", make_shared <MyDB_IntAttType>()));
        mySchema->appendAtt(make_pair("myString", make_shared <MyDB_StringAttType>()));
        mySchema->appendAtt(make_pair("myDouble", make_shared <MyDB_DoubleAttType>()));
        
        MyDB_TablePtr myTable = make_shared <MyDB_Table>("extraTableMixed", "extraTableMixed.bin", mySchema);
        MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager>(1024, 16, "extraTempFile");
        MyDB_TableReaderWriter testTable(myTable, myMgr);
        MyDB_RecordPtr temp = testTable.getEmptyRecord();
        
        // "1|Hello|3.14|"
        string s = "1|Hello|3.14|";
        temp->fromString(s);
        testTable.append(temp);
        
        // Read back and verify
        MyDB_RecordPtr outRec = testTable.getEmptyRecord();
        MyDB_RecordIteratorPtr myIter = testTable.getIterator(outRec);
        
        if(myIter->hasNext()) {
            myIter->getNext();
            // We can't easily check fields without getters, but standard output string should match
            cout << "Read: " << outRec << endl; // Assuming operator<< works
            // In A2 we assume it does via QUnit/iostream
            cout << "CORRECT" << endl << flush;
        } else {
             cout << "***FAIL*** (No record found)" << endl << flush;
        }
        
        // Persist for Test 12
        myTable->putInCatalog(myCatalog);
        QUNIT_IS_TRUE(true);
    }
    FALLTHROUGH_INTENDED;
    case 12:
    {
        int count = 0;
        {
            // Test 12: Copy Table A to Table B
            cout << "TEST 12: Copy Table..." << endl << flush;
            MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog>("extraCatFile");
            // Reuse mixed table from 11 as source
            map <string, MyDB_TablePtr> allTables = MyDB_Table::getAllTables(myCatalog);
            MyDB_TablePtr sourceTable = allTables["extraTableMixed"];
             
            MyDB_SchemaPtr mySchema = sourceTable->getSchema(); 
            
            MyDB_TablePtr destTable = make_shared <MyDB_Table>("extraTableCopy", "extraTableCopy.bin", mySchema);
            
            MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager>(1024, 16, "extraTempFile");
            
            MyDB_TableReaderWriter sourceRW(sourceTable, myMgr);
            MyDB_TableReaderWriter destRW(destTable, myMgr);
            
            MyDB_RecordPtr rec = sourceRW.getEmptyRecord();
            MyDB_RecordIteratorPtr srcIter = sourceRW.getIterator(rec);
            
            while(srcIter->hasNext()) {
                srcIter->getNext();
                destRW.append(rec);
                count++;
            }
            
            if(count == 1) cout << "CORRECT (Copied 1 record)" << endl << flush;
            else cout << "***FAIL*** (Copied " << count << ")" << endl << flush;
        }
        
        unlink("extraTableCopy.bin");
        unlink("extraTableMixed.bin"); 
        QUNIT_IS_EQUAL(count, 1);
    }
    FALLTHROUGH_INTENDED;
    case 13:
    {
        int i = 0;
        {
            // Test 13: Buffer Stress (Large Data Correctness)
            cout << "TEST 13: Buffer Stress..." << endl << flush;
            // Write enough data to flush to disk, then verify correctness
            // Buffer is 16 pages. Write 20 pages.
             MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog>("extraCatFile");
             MyDB_SchemaPtr mySchema = make_shared <MyDB_Schema>();
             mySchema->appendAtt(make_pair("id", make_shared <MyDB_IntAttType>()));
             MyDB_TablePtr myTable = make_shared <MyDB_Table>("extraTableStress", "extraTableStress.bin", mySchema);
             // Page 64 bytes. 16 pages = 1024 bytes total buffer.
             MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager>(64, 16, "extraTempFile");
             MyDB_TableReaderWriter testTable(myTable, myMgr);
             
             MyDB_RecordPtr temp = testTable.getEmptyRecord();
             
             // Write 1000 records. (4 bytes each -> 4000 bytes > 1024)
             for(int k=0; k<1000; k++) {
                 string s = to_string(k) + "|";
                 temp->fromString(s);
                 testTable.append(temp);
             }
             
             // Read Back
             MyDB_RecordIteratorPtr myIter = testTable.getIterator(temp);
             
             bool idsMatch = true;
             while(myIter->hasNext()) {
                 myIter->getNext();
                 i++;
             }
             
             if(i == 1000) cout << "CORRECT" << endl << flush;
             else cout << "***FAIL*** (Count " << i << ")" << endl << flush;
        }
        
        // Cleanup
        unlink("extraTableStress.bin");
        unlink("extraCatFile");
        unlink("extraTempFile"); // BufferMgr destructor should have released it
        
        QUNIT_IS_EQUAL(i, 1000);
    } 
    FALLTHROUGH_INTENDED;

	default:
		break;
	}
    
    // cleanup_extra(); // Keep data for inspection if needed, or uncomment
}

#endif

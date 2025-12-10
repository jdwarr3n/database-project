
#ifndef PERFORMANCE_UNIT_H
#define PERFORMANCE_UNIT_H

#include "MyDB_BufferManager.h"
#include "MyDB_PageHandle.h"
#include "MyDB_Table.h"
#include "QUnit.h"
#include <iostream>
#include <unistd.h>
#include <vector>
#include <ctime>
#include <string>

using namespace std;

// Helper to run a unified performance test scenario
void runPerfTest(QUnit::UnitTest& qunit, string testName, int bufferSize, int totalOps, double timeoutSecs) {
    cout << "--------------------------------------------------" << endl;
    cout << "TEST: " << testName << endl;
    cout << "Params: Buffer=" << bufferSize << ", Ops=" << totalOps << ", Timeout=" << timeoutSecs << "s" << endl;
    
    MyDB_BufferManager myMgr (64, bufferSize, "tempPerf_" + testName);
    MyDB_TablePtr table1 = make_shared <MyDB_Table> ("tempTablePerf", "perfData");
    
    // Pre-load pages to ensure cache hits
    vector<MyDB_PageHandle> handles;
    for (int i = 0; i < bufferSize; i++) {
         handles.push_back(myMgr.getPage(table1, i));
    }
    
    clock_t begin = clock();
    
    for (int i = 0; i < totalOps; i++) {
        // Access random page within the buffer to trigger move-to-MRU
        long pageId = (i * 7 + 13) % bufferSize;
        myMgr.getPage(table1, pageId);
        
        // Periodic timeout check (every 10k ops)
        if (i % 10000 == 0) {
             double current_time = double(clock() - begin) / CLOCKS_PER_SEC;
             if (current_time > timeoutSecs) {
                 cout << "FAILURE: " << testName << " timed out!" << endl;
                 cout << "Elapsed: " << current_time << "s (Limit: " << timeoutSecs << "s)" << endl;
                 QUNIT_IS_TRUE(false);
                 return; 
             }
        }
    }
    
    clock_t end = clock();
    double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
    
    cout << "PASS: " << testName << " completed." << endl;
    cout << "Time: " << elapsed_secs << "s (Limit: " << timeoutSecs << "s)" << endl;
    cout << "Throughput: " << (elapsed_secs * 1000000 / totalOps) << " us/op" << endl;
    
    QUNIT_IS_TRUE(elapsed_secs < timeoutSecs);
}

int main () {

	QUnit::UnitTest qunit(cerr, QUnit::verbose);

	{
        cout << "Starting Final Performance Test Suite..." << endl;
        
        // Test 1: Baseline (100k Buffer, 100k Ops)
        // Expected: ~0.08s. Timeout: 0.2s.
        runPerfTest(qunit, "Baseline_100k_100k", 100000, 100000, 0.2);
        
        // Test 2: Medium Load (100k Buffer, 1M Ops)
        // Expected: ~0.75s. Timeout: 1.5s.
        runPerfTest(qunit, "Medium_100k_1M", 100000, 1000000, 1.5);
        
        // Test 3: Large Load (100k Buffer, 5M Ops)
        // Expected: ~3.75s. Timeout: 7.5s.
        runPerfTest(qunit, "Large_100k_5M", 100000, 5000000, 7.5); // 2x expected ~3.75s
        
        // Test 4: Small Buffer Stress (1k Buffer, 1M Ops)
        // Verifies O(1) holds even when list is smaller (mostly overhead check)
        // Expected: ~0.75s. Timeout: 1.5s.
        runPerfTest(qunit, "SmallBuf_1k_1M", 1000, 1000000, 1.5);
	}
    
    cout << "All Performance Tests Complete." << endl;
}

#endif

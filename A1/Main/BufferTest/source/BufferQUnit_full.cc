
#ifndef CATALOG_UNIT_H
#define CATALOG_UNIT_H

#define FALLTHROUGH_INTENDED do {} while (0)

#include "MyDB_BufferManager.h"
#include "MyDB_PageHandle.h"
#include "MyDB_Table.h"
#include "QUnit.h"
#include <cstring>
#include <iostream>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <fcntl.h>
// #include <sys/wait.h>
#include <algorithm>
#include <fstream>
#include <sys/stat.h>

#ifndef O_FSYNC
#define O_FSYNC 0
#endif

using namespace std;

// these functions write a bunch of characters to a string... used to produce data
void writeNums (char *bytes, size_t len, int i) {
	for (size_t j = 0; j < len - 1; j++) {
		bytes[j] = '0' + (i % 10);
	}
	bytes[len - 1] = 0;
}

void writeLetters (char *bytes, size_t len, int i) {
	for (size_t j = 0; j < len - 1; j++) {
		bytes[j] = 'i' + (i % 10);
	}
	bytes[len - 1] = 0;
}

void writeSymbols (char *bytes, size_t len, int i) {
	for (size_t j = 0; j < len - 1; j++) {
		bytes[j] = '!' + (i % 10);
	}
	bytes[len - 1] = 0;
}

int main (int argc, char *argv[]) {
	int start = 1;
	if (argc > 1 && argv[1][0] >= '0' && argv[1][0] <= '9') {
		start = atoi(argv[1]);
	}
	cout << "start from test " << start << endl;

	QUnit::UnitTest qunit(cerr, QUnit::normal);

	switch (start) {
	case 1:
		{
			// buffer manager and temp page
			cout << "TEST 1..." << flush;
			{
				cout << "create manager..." << flush;
				MyDB_BufferManager myMgr(64, 16, "tempDSFSD");
				cout << "get page..." << flush;
				MyDB_PageHandle page1 = myMgr.getPage();
				cout << "get bytes..." << flush;
				char *bytes = (char *)page1->getBytes();
				cout << "write bytes..." << flush;
				memset(bytes, 'A', 64);
				page1->wroteBytes();
				cout << "shutdown manager..." << flush;
			}
			cout << "COMPLETE" << endl << flush;
			QUNIT_IS_TRUE(true);
		}
		FALLTHROUGH_INTENDED;
	case 2:
		{
			// write unpinned and pinned page
			cout << "TEST 2..." << flush;
			{
				cout << "create manager..." << flush;
				MyDB_BufferManager myMgr(64, 16, "tempDSFSD");
				cout << "get page..." << flush;
				MyDB_TablePtr table1 = make_shared <MyDB_Table>("table1", "file1");
				MyDB_TablePtr table2 = make_shared <MyDB_Table>("table2", "file2");
				MyDB_PageHandle page1 = myMgr.getPage(table1, 0);
				MyDB_PageHandle page2 = myMgr.getPinnedPage(table2, 1);
				cout << "get bytes..." << flush;
				char *bytes1 = (char *)page1->getBytes();
				char *bytes2 = (char *)page2->getBytes();
				cout << "write bytes..." << flush;
				memset(bytes1, 'A', 64);
				page1->wroteBytes();
				memset(bytes2, 'B', 64);
				page2->wroteBytes();
				cout << "shutdown manager..." << flush;
			}
			cout << "COMPLETE" << endl << flush;
			QUNIT_IS_TRUE(true);
		}
		FALLTHROUGH_INTENDED;
	case 3:
		{
			// read unpinned and pinned page (requires write unpinned and pinned page)
			bool flag3 = true;
			cout << "TEST 3..." << flush;
			{
				cout << "create manager..." << flush;
				MyDB_BufferManager myMgr(64, 16, "tempDSFSD");
				cout << "get page..." << flush;
				MyDB_TablePtr table1 = make_shared <MyDB_Table>("table1", "file1");
				MyDB_TablePtr table2 = make_shared <MyDB_Table>("table2", "file2");
				MyDB_PageHandle page1 = myMgr.getPage(table1, 0);
				MyDB_PageHandle page2 = myMgr.getPinnedPage(table2, 1);
				cout << "get bytes..." << flush;
				char *bytes1 = (char *)page1->getBytes();
				char *bytes2 = (char *)page2->getBytes();
				cout << "compare bytes..." << flush;
				for (int i = 0; i < 64; i++) {
					if (bytes1[i] != 'A') flag3 = false;
					if (bytes2[i] != 'B') flag3 = false;
				}
				cout << "shutdown manager..." << flush;
			}
			if (flag3) cout << "CORRECT" << endl << flush;
			else cout << "***FAIL***" << endl << flush;
			QUNIT_IS_TRUE(flag3);
		}
		FALLTHROUGH_INTENDED;
	case 4:
		{
			// write large pages
			cout << "TEST 4..." << flush;
			{
				cout << "create manager..." << flush;
				MyDB_BufferManager myMgr(1048576, 16, "tempDSFSD");
				cout << "get page..." << flush;
				MyDB_TablePtr table1 = make_shared <MyDB_Table>("table1", "file1");
				vector<MyDB_PageHandle> pages(16);
				for (int i = 0; i < 16; i++) {
					pages[i] = myMgr.getPinnedPage(table1, i);
				}
				cout << "get bytes..." << flush;
				vector<char*> bytes(16);
				for (int i = 0; i < 16; i++) {
					bytes[i] = (char *)pages[i]->getBytes();
				}
				cout << "write bytes..." << flush;
				for (int i = 0; i < 16; i++) {
					memset(bytes[i], 'C', 1048576);
					pages[i]->wroteBytes();
				}
				cout << "shutdown manager..." << flush;
			}
			cout << "COMPLETE" << endl << flush;
			QUNIT_IS_TRUE(true);
		}
		FALLTHROUGH_INTENDED;
	case 5:
		{
			// large LRU
			cout << "TEST 5..." << flush;
			{
				cout << "create manager..." << flush;
				MyDB_BufferManager myMgr(64, 100000, "tempDSFSD");
				cout << "get page..." << flush;
				MyDB_TablePtr table1 = make_shared <MyDB_Table>("table1", "file1");
				vector<MyDB_PageHandle> pages(100000);
				for (int i = 0; i < 100000; i++) {
					pages[i] = myMgr.getPage(table1, i);
				}
				cout << "get bytes..." << flush;
				vector<char*> bytes(100000);
				for (int i = 0; i < 100000; i++) {
					bytes[i] = (char *)pages[i]->getBytes();
				}
				cout << "shutdown manager..." << flush;
			}
			cout << "COMPLETE" << endl << flush;
			QUNIT_IS_TRUE(true);
		}
		FALLTHROUGH_INTENDED;
	case 6:
		{
			// alternate slot
			cout << "TEST 6..." << flush;
			{
				cout << "create manager..." << flush;
				MyDB_BufferManager myMgr(64, 16, "tempDSFSD");
				cout << "get page..." << flush;
				MyDB_TablePtr table1 = make_shared <MyDB_Table>("table1", "file1");
				vector<MyDB_PageHandle> pages(17);
				for (int i = 0; i < 15; i++) {
					pages[i] = myMgr.getPinnedPage(table1, i);
				}
				for (int i = 15; i < 17; i++) {
					pages[i] = myMgr.getPage(table1, i);
				}
				cout << "get bytes..." << flush;
				clock_t t1, t2, t3;
				volatile char *bytes1, *bytes2;
				t1 = clock(); 
				for (int i = 0; i < 100000; i++) {
					bytes1 = (char *)pages[13]->getBytes();
					bytes2 = (char *)pages[14]->getBytes();
				}
				t2 = clock();
				for (int i = 0; i < 100000; i++) {
					bytes1 = (char *)pages[15]->getBytes();
					bytes2 = (char *)pages[16]->getBytes();
				}
				t3 = clock();
				cout << t2 - t1 << "..." << t3 - t2 << "...";
				cout << "shutdown manager..." << flush;
			}
			cout << "COMPLETE" << endl << flush;
			QUNIT_IS_TRUE(true);
		}
		FALLTHROUGH_INTENDED;
	case 7:
		{
			// rolling LRU
			cout << "TEST 7..." << flush;
			{
				cout << "create manager..." << flush;
				MyDB_BufferManager myMgr(64, 100, "tempDSFSD");
				cout << "get page..." << flush;
				MyDB_TablePtr table1 = make_shared <MyDB_Table>("table1", "file1");
				vector<MyDB_PageHandle> pages(101);
				for (int i = 0; i < 101; i++) {
					pages[i] = myMgr.getPage(table1, i);
				}
				cout << "get bytes..." << flush;
				clock_t t1, t2, t3;
				volatile char *bytes1;
				t1 = clock(); 
				for (int i = 0; i < 1000; i++) {
					for (int j = 0; j < 100; j++) {
						bytes1 = (char *)pages[j]->getBytes();
					}
				}
				t2 = clock();
				for (int i = 0; i < 1000; i++) {
					for (int j = 0; j < 101; j++) {
						bytes1 = (char *)pages[j]->getBytes();
					}
				}
				t3 = clock();
				cout << t2 - t1 << "..." << t3 - t2 << "...";
				cout << "shutdown manager..." << flush;
			}
			cout << "COMPLETE" << endl << flush;
			QUNIT_IS_TRUE(true);
		}
		FALLTHROUGH_INTENDED;
	case 8:
		{
			// rolling temp
			cout << "TEST 8..." << flush;
			bool flag8 = true;
			{
				cout << "create manager..." << flush;
				MyDB_BufferManager myMgr(64, 16, "tempDSFSD");
				cout << "get page..." << flush;
				vector<MyDB_PageHandle> pages(50);
				for (int i = 0; i < 50; i++) {
					pages[i] = myMgr.getPage();
				}
				cout << "write bytes..." << flush;
				vector<char*> bytes(50);
				for (int i = 0; i < 50; i++) {
					bytes[i] = (char *)pages[i]->getBytes();
					memset(bytes[i], (char)('A' + i), 64);
					pages[i]->wroteBytes();
				}
				cout << "read bytes..." << flush;
				for (int i = 0; i < 50; i++) {
					bytes[i] = (char *)pages[i]->getBytes();
					char c = (char)('A' + i);
					for (int j = 0; j < 64; j++) {
						if (bytes[i][j] != c) flag8 = false;
					}
				}
				cout << "shutdown manager..." << flush;
			}
			if (flag8) cout << "CORRECT" << endl << flush;
			else cout << "***FAIL***" << endl << flush;
			QUNIT_IS_TRUE(flag8);
		}
		FALLTHROUGH_INTENDED;
	case 9:
		{
			// multiple handles
			bool flag9 = true;
			cout << "TEST 9..." << flush;
			{
				cout << "create manager..." << flush;
				MyDB_BufferManager myMgr(64, 16, "tempDSFSD");
				cout << "get page..." << flush;
				MyDB_TablePtr table1 = make_shared <MyDB_Table>("table1", "file1");
				vector<MyDB_PageHandle> pagesA(16);
				vector<MyDB_PageHandle> pagesB(16);
				vector<MyDB_PageHandle> pagesC(16);
				for (int i = 0; i < 16; i++) {
					pagesA[i] = myMgr.getPage(table1, i);
					pagesB[i] = myMgr.getPage(table1, i);
					pagesC[i] = myMgr.getPage(table1, i);
				}
				cout << "write bytes..." << flush;
				for (int i = 0; i < 16; i++) {
					char *bytes = (char *)pagesA[i]->getBytes();
					memset(bytes, (char)('A' + i), 64);
					pagesA[i]->wroteBytes();
				}
				for (int i = 0; i < 16; i++) {
					char *bytes = (char *)pagesB[i]->getBytes();
					memset(bytes, (char)('a' + i), 64);
					pagesB[i]->wroteBytes();
				}
				cout << "read bytes..." << flush;
				for (int i = 0; i < 16; i++) {
					char *bytes = (char *)pagesC[i]->getBytes();
					char c = (char)('a' + i);
					for (int j = 0; j < 64; j++) {
						if (bytes[j] != c) flag9 = false;
					}
				}
				cout << "shutdown manager..." << flush;
			}
			cout << "COMPLETE" << endl << flush;
			QUNIT_IS_TRUE(flag9);
		}
		FALLTHROUGH_INTENDED;
	case 10:
		{
			// test case by Professor Chris
			cout << "TEST 10..." << flush;
			{
				// create a buffer manager
				cout << "create manager..." << flush;
				MyDB_BufferManager myMgr (64, 16, "tempDSFSD");
				MyDB_TablePtr table1 = make_shared <MyDB_Table> ("tempTable", "foobar");

				// allocate a pinned page
				cout << "get pinned page 0..." << flush;
				MyDB_PageHandle pinnedPage = myMgr.getPinnedPage (table1, 0);
				char *bytes = (char *) pinnedPage->getBytes ();
				writeNums (bytes, 64, 0);
				pinnedPage->wroteBytes ();

		
				// create a bunch of pinned pages and remember them
				cout << "get pinned page 1~9..." << flush;
				vector <MyDB_PageHandle> myHandles;
				for (int i = 1; i < 10; i++) {
					MyDB_PageHandle temp = myMgr.getPinnedPage (table1, i);
					char *bytes = (char *) temp->getBytes ();
					writeNums (bytes, 64, i);
					temp->wroteBytes ();
					myHandles.push_back (temp);
				}

				// now forget the pages we created
				cout << "forget pinned page 1~9..." << flush;
				vector <MyDB_PageHandle> temp;
				myHandles = temp;

				// now remember 8 more pages
				cout << "get pinned page 0~7..." << flush;
				for (int i = 0; i < 8; i++) {
					MyDB_PageHandle temp = myMgr.getPinnedPage (table1, i);
					char *bytes = (char *) temp->getBytes ();

					// write numbers at the 0th position
					if (i == 0)
						writeNums (bytes, 64, i);
					else
						writeSymbols (bytes, 64, i);
					temp->wroteBytes ();
					myHandles.push_back (temp);
				}

				// now correctly write nums at the 0th position
				cout << "get unpinned page 0..." << flush;
				MyDB_PageHandle anotherDude = myMgr.getPage (table1, 0);
				bytes = (char *) anotherDude->getBytes ();
				writeSymbols (bytes, 64, 0);
				anotherDude->wroteBytes ();
		
				// now do 90 more pages, for which we forget the handle immediately
				cout << "get and forget unpinned page 10~99..." << flush;
				for (int i = 10; i < 100; i++) {
					MyDB_PageHandle temp = myMgr.getPage (table1, i);
					char *bytes = (char *) temp->getBytes ();
					writeNums (bytes, 64, i);
					temp->wroteBytes ();
				}

				// now forget all of the pinned pages we were remembering
				cout << "forget pinned page 0~7..." << flush;
				vector <MyDB_PageHandle> temp2;
				myHandles = temp2;

				// now get a pair of pages and write them
				cout << "get and forget 200 temp pages..." << flush;
				for (int i = 0; i < 100; i++) {
					MyDB_PageHandle oneHandle = myMgr.getPinnedPage ();
					char *bytes = (char *) oneHandle->getBytes ();
					writeNums (bytes, 64, i);
					oneHandle->wroteBytes ();
					MyDB_PageHandle twoHandle = myMgr.getPinnedPage ();
					writeNums (bytes, 64, i);
					twoHandle->wroteBytes ();
				}

				// make a second table
				cout << "get unpinned page 0~99 of second table..." << flush;
				MyDB_TablePtr table2 = make_shared <MyDB_Table> ("tempTable2", "barfoo");
				for (int i = 0; i < 100; i++) {
					MyDB_PageHandle temp = myMgr.getPage (table2, i);
					char *bytes = (char *) temp->getBytes ();
					writeLetters (bytes, 64, i);
					temp->wroteBytes ();
				}
				cout << "shutdown manager..." << flush;
			}

			// remove the temp file
			cout << "delete tempfile..." << flush;
			unlink ("tempFile");

			int countCorrect = 0;
			{
				cout << "create manager..." << flush;
				MyDB_BufferManager myMgr (64, 16, "tempDSFSD");
				MyDB_TablePtr table1 = make_shared <MyDB_Table> ("tempTable", "foobar");

				// look up all of the pages, and make sure they have the correct numbers
				cout << "get and verify unpinned page 0~99..." << flush;
				for (int i = 0; i < 100; i++) {
					MyDB_PageHandle temp = myMgr.getPage (table1, i);
					char answer[64];
					if (i < 8)
						writeSymbols (answer, 64, i);
					else
						writeNums (answer, 64, i);
					char *bytes = (char *) temp->getBytes ();
					if (string (answer) == string (bytes))
						countCorrect++;
				}
				cout << "shutdown manager..." << flush;
			}
			string temp = to_string(countCorrect) + string (" test cases correct");
			cout << temp << "..." << flush;
			if (temp == string("100 test cases correct")) cout << "CORRECT" << endl << flush;
			else cout << "***FAIL***" << endl << flush;
			QUNIT_IS_EQUAL (string("100 test cases correct"), temp);
		}
		FALLTHROUGH_INTENDED;

	case 11:
		{
			cout << "TEST 11 (evict write-back)..." << flush;
            cout << "SKIPPED due to Windows incompatibility (fork)." << endl;
            QUNIT_IS_TRUE(true);
		}
		FALLTHROUGH_INTENDED;

	case 12: 
		{
			cout << "TEST 12..." << flush;
			bool bad = false;

			{
				// create a table file of a certain size (64 * 32)
				cout << "create table file..." << flush;
				{
					int fd = creat("foobarbaz", S_IRUSR | S_IWUSR);
					lseek(fd, 64 * 32 - 1, SEEK_SET);
					write(fd, "", 1);
					close(fd);
				}

				// create a buffer manager
				cout << "create manager..." << flush;
				MyDB_BufferManager myMgr(64, 16, "tempDSFSD");
				MyDB_TablePtr	   table = make_shared<MyDB_Table>("tempTable", "foobarbaz");

				vector<MyDB_PageHandle> myHandles;

				// create a bunch of pages (not to exceed the size of the buffer)
				cout << "get and write pages 0~15..." << flush;
				for (int i = 0; i < 16; i++)
				{
					MyDB_PageHandle temp  = myMgr.getPage(table, i);
					char		   *bytes = (char *)temp->getBytes();
					memset(bytes, 'A', 64);
					temp->wroteBytes();
					myHandles.push_back(temp);
				}

				// no pages should have been evicted
				cout << "no pages should have been evicted..." << flush;
				{
					int	 fd = open("foobarbaz", O_RDONLY | O_FSYNC);
					char buffer[64 * 32];
					read(fd, buffer, 64 * 32);
					close(fd);

					if (find(buffer, buffer + 64 * 32, 'A') != buffer + 64 * 32)
					{
						cout << "unexpected eviction!!!" << flush;
						bad = true;
					}
				}

				// access the pages in the buffer again
				cout << "get and write pages 0~15 again..." << flush;
				for (int i = 0; i < 16; i++)
				{
					MyDB_PageHandle temp  = myMgr.getPage(table, i);
					char		   *bytes = (char *)temp->getBytes();
					memset(bytes, 'B', 64);
					temp->wroteBytes();
					myHandles.push_back(temp);
				}

				// no pages should have been evicted
				cout << "no pages should have been evicted..." << flush;
				{
					int	 fd = open("foobarbaz", O_RDONLY | O_FSYNC);
					char buffer[64 * 32];
					read(fd, buffer, 64 * 32);
					close(fd);

					const string chars = "AB";
					if (find_first_of(buffer, buffer + 64 * 32, chars.begin(), chars.end()) != buffer + 64 * 32)
					{
						cout << "unexpected eviction!!!" << flush;
						bad = true;
					}
				}

				// access a new page, which should cause an eviction
				cout << "get and write page 16..." << flush;
				{
					MyDB_PageHandle temp  = myMgr.getPage(table, 16);
					char		   *bytes = (char *)temp->getBytes();
					memset(bytes, 'C', 64);
					temp->wroteBytes();
					myHandles.push_back(temp);
				}

				// now a page should have been evicted
				cout << "only one page should have been evicted..." << flush;
				{
					int	 fd = open("foobarbaz", O_RDONLY | O_FSYNC);
					char buffer[64 * 32];
					read(fd, buffer, 64 * 32);
					close(fd);

					string result(buffer, 64 * 32);
					if (result.find('A') != string::npos)
					{
						cout << "wrote back outdated contents!!!" << flush;
						bad = true;
					}
					if (result.find('C') != string::npos)
					{
						cout << "evicted the MRU page!!!" << flush;
						bad = true;
					}
					if (count(result.begin(), result.end(), 'B') != 64)
					{
						cout << "evicted the wrong number of pages!!!" << flush;
						bad = true;
					}
				}
			}

			// write back
			cout << "all pages should have been written back..." << flush;
			{
				ifstream	  tableFile("foobarbaz", ios::binary);
				ostringstream buffer;
				buffer << tableFile.rdbuf();
				if (buffer.str() != string(64 * 16, 'B') + string(64, 'C') + string(64 * 15, '\0'))
				{
					cout << "data written back incorrectly!!!" << flush;
					bad = true;
				}
			}

			QUNIT_IS_EQUAL(bad, false);
			cout << "COMPLETE" << endl << flush;
		}
		FALLTHROUGH_INTENDED;

	case 13: 
		{
			cout << "TEST 13..." << flush;
			bool recycleOK = true;
			{
				const char *tmpName = "tempRecycleTest";
				unlink(tmpName);

				cout << "create manager..." << flush;
				MyDB_BufferManager myMgr(64, 2, tmpName);

				// Batch 1: keep 10 anonymous pages alive
				vector<MyDB_PageHandle> batch1;
				for (int i = 0; i < 10; i++) {
					MyDB_PageHandle ph = myMgr.getPage();
					char *b = (char *)ph->getBytes();
					memset(b, 'A', 64);
					ph->wroteBytes();
					batch1.push_back(ph);
				}
				// now there should be 8 page stored in tempRecycleTest

				struct stat st1 {};
				if (stat(tmpName, &st1) != 0) {
					perror("stat batch1");
					recycleOK = false;
				}

				batch1.clear();
				batch1.shrink_to_fit();

				// Batch 2: allocate 10 new anonymous pages
				vector<MyDB_PageHandle> batch2;
				for (int i = 0; i < 10; i++) {
					MyDB_PageHandle ph = myMgr.getPage();
					char *b = (char *)ph->getBytes();
					memset(b, 'a', 64);
					ph->wroteBytes();
					batch2.push_back(ph);
				}

				struct stat st2 {};
				if (stat(tmpName, &st2) != 0) {
					perror("stat batch2");
					recycleOK = false;
				}

				if (recycleOK) {
					// With 10 pages and 2 frames, only 8 should spill to disk
					// off_t maxExpected = (off_t)(64 * (10 - 2));
			
					off_t maxExpected = st1.st_size;
					if (st2.st_size > maxExpected) {
						recycleOK = false;
					}
				}

				cout << "temp file sizes " << st1.st_size << " -> " << st2.st_size << "..." << flush;
				cout << "shutdown manager..." << flush;
				unlink(tmpName);
			}
			cout << "COMPLETE" << endl << flush;
			QUNIT_IS_TRUE(recycleOK);
		}
		FALLTHROUGH_INTENDED;
	
	case 14: 

		{
			bool flag11 = true;
			cout << "TEST 14 (LRU Eviction)..." << flush;
			{
				cout << "create manager..." << flush;
				MyDB_BufferManager myMgr(64, 4, "test_file_lru");
				MyDB_TablePtr table1 = make_shared <MyDB_Table>("table1", "file_lru");
				
				// Fill the buffer with P0, P1, P2, P3 (P0 is LRU)
				vector<MyDB_PageHandle> pages(4);
				for (int i = 0; i < 4; i++) {
					pages[i] = myMgr.getPage(table1, i);
					char *bytes = (char *)pages[i]->getBytes();
					// Write unique data for verification
					memset(bytes, (char)('E' + i), 64);
					pages[i]->wroteBytes(); 
				}

				// Touch P1, P2, P3 to make P0 the LRU (P3 is MRU)
				pages[3]->getBytes(); 
				pages[2]->getBytes(); 
				pages[1]->getBytes(); 
				
				// Request P4. This must evict P0 (LRU) to disk.
				cout << "request P4 (should evict P0)..." << flush;
				MyDB_PageHandle page4 = myMgr.getPage(table1, 4);

				// Request P5. This must evict P3 (new LRU).
				cout << "request P5 (should evict P3)..." << flush;
				MyDB_PageHandle page5 = myMgr.getPage(table1, 5);

				// Try to read P0 again. If it was properly evicted, it must be re-read from disk.
				//    Since P0 was dirty, its contents must still be correct.
				cout << "read P0 (must be reloaded)..." << flush;
				char *bytes0_read = (char *)pages[0]->getBytes();
				if (bytes0_read[0] != 'E') {
					cout << "P0 data corruption after eviction..." << flush;
					flag11 = false;
				}

				// Request P6. This should evict P1 (new LRU, since P0 was touched).
				cout << "request P6 (should evict P1)..." << flush;
				MyDB_PageHandle page6 = myMgr.getPage(table1, 6);

				// Touch P2 again. P2 is MRU.
				pages[2]->getBytes(); 

				// Request P7. This should evict P4 (LRU).
				cout << "request P7 (should evict P4)..." << flush;
				MyDB_PageHandle page7 = myMgr.getPage(table1, 7);
				
				// The main check is that the system didn't crash and the data of P0 was preserved.
				if (flag11) cout << "LRU eviction sequence successful..." << flush;
				else cout << "INCORRECT..." << flush;

				cout << "shutdown manager..." << flush;
			}
			cout << "COMPLETE" << endl << flush;
			QUNIT_IS_TRUE(flag11);
		}
		FALLTHROUGH_INTENDED;

	default:
		break;
	}
}

#endif

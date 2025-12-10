
#ifndef BUFFER_MGR_H
#define BUFFER_MGR_H

#include "MyDB_PageHandle.h"
#include "MyDB_Table.h"
#include "MyDB_Page.h"
#include <map>
#include <list>
#include <vector>
#include <set>

using namespace std;

class MyDB_BufferManager {

public:
    friend class MyDB_Page;

	// THESE METHODS MUST APPEAR AND THE PROTOTYPES CANNOT CHANGE!

	// gets the i^th page in the table whichTable... note that if the page
	// is currently being used (that is, the page is current buffered) a handle 
	// to that already-buffered page should be returned
	MyDB_PageHandle getPage (MyDB_TablePtr whichTable, long i);

	// gets a temporary page that will no longer exist (1) after the buffer manager
	// has been destroyed, or (2) there are no more references to it anywhere in the
	// program.  Typically such a temporary page will be used as buffer memory.
	// since it is just a temp page, it is not associated with any particular 
	// table
	MyDB_PageHandle getPage ();

	// gets the i^th page in the table whichTable... the only difference 
	// between this method and getPage (whicTable, i) is that the page will be 
	// pinned in RAM; it cannot be written out to the file
	MyDB_PageHandle getPinnedPage (MyDB_TablePtr whichTable, long i);

	// gets a temporary page, like getPage (), except that this one is pinned
	MyDB_PageHandle getPinnedPage ();

	// un-pins the specified page
	void unpin (MyDB_PageHandle unpinMe);

    // Maintenance method to remove specific page from buffer (used by anonymous pages)
    void killPage(MyDB_Page* page);
    
    // Method to re-load an evicted page (called by MyDB_Page::getBytes)
    void repin(MyDB_Page* page);

	// creates an LRU buffer manager... params are as follows:
	// 1) the size of each page is pageSize 
	// 2) the number of pages managed by the buffer manager is numPages;
	// 3) temporary pages are written to the file tempFile
	MyDB_BufferManager (size_t pageSize, size_t numPages, string tempFile);
	
	// when the buffer manager is destroyed, all of the dirty pages need to be
	// written back to disk, any necessary data needs to be written to the catalog,
	// and any temporary files need to be deleted
	~MyDB_BufferManager ();

	// FEEL FREE TO ADD ADDITIONAL PUBLIC METHODS 

private:

	// YOUR STUFF HERE
    
    // Core settings
    size_t pageSize;
    size_t numPages;
    string tempFile;
    
    // Special table for anonymous pages
    MyDB_TablePtr tempTable;
    long lastTempId;
    
    // The actual memory buffer
    // We treat it as an array of page objects or just raw bytes?
    // Better to have vector<char> or char* buffer;
    char *buffer; // The big chunk
    
    // Tracking Free RAM
    // A list of free buffer slot indices (0 to numPages-1)
    vector<long> freeSlots;
    
    // Tracking Anonymous File Slots
    // Just a counter? Recycled slots?
    // "disk slot associated with the anonymous page should be recycled"
    set<long> freeTempSlots;
    
    // Page Lookup
    // Key: {TablePtr, PageID} -> Value: Page Object
    map<pair<MyDB_TablePtr, long>, MyDB_PagePtr> allPages;
    
    // LRU List
    // We need to move accessed pages to the back (MRU).
    // Evict from front (LRU).
    list<MyDB_PagePtr> lruList;
    
    // Helper to common logic (get slot, load data, etc)
    // MyDB_PageHandle _getPage(MyDB_TablePtr table, long i, bool pinned);
    
    // Helper to find a victim and evict
    // returns pointer to the buffer slot (char*) that was freed up
    char* evict ();
    
    // Helper to get a buffer slot (from free list or by eviction)
    char* getBufferSlot ();

};

#endif

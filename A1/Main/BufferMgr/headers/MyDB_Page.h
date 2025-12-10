
#ifndef PAGE_H
#define PAGE_H

#include <string>
#include "MyDB_Table.h"
#include <memory> 

using namespace std;

class MyDB_Page;
typedef shared_ptr <MyDB_Page> MyDB_PagePtr;
class MyDB_BufferManager;

// instances of this class are used to store the actual data held by a page
class MyDB_Page {

public:

	// access the raw bytes in this page
	void *getBytes ();

	// let the page know that we have written to the bytes
	void wroteBytes ();

	// constructor
	MyDB_Page (MyDB_TablePtr myTable, long i, void *bytes, bool pinned, size_t pageSize, MyDB_BufferManager *parent);
    
    // parent manager
    MyDB_BufferManager *parent;

	// destructor
	~MyDB_Page ();

	// kill the page... this means that any dirty data is written back
	// to disk, and the page is marked as invalid
	void writeBack ();

	// setup methods for ref counting
	void incrementRefCount ();
	void decrementRefCount ();
    int getRefCount ();

	// getters
	bool isPinnedPtr (); 
	bool isDirtyPtr ();
    MyDB_TablePtr getTable ();
    long getPos ();
    
    // setters
    void setUnpinned ();

private:

	friend class MyDB_BufferManager;
	friend class MyDB_PageHandleBase;

	// the table that this page belongs to
	MyDB_TablePtr myTable;

	// the position of this page in the table
	long pos;

	// the raw bytes in this page
	void *bytes;

	// is this page dirty?
	bool isDirty;

	// is this page pinned?
	bool isPinned;

	// the number of references to this page
	int refCount;
    
    // the size of the page
    size_t pageSize;
};

#endif

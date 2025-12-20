
#ifndef PAGE_REC_ITER_H
#define PAGE_REC_ITER_H

#include "MyDB_RecordIterator.h"
#include "MyDB_PageHandle.h"
#include "MyDB_Record.h"

class MyDB_PageRecIterator : public MyDB_RecordIterator {

public:

	// constructor
	MyDB_PageRecIterator (MyDB_PageHandle myPage, MyDB_RecordPtr myRec);

	// put the contents of the next record in the file/page into the iterator record
	// this should be called BEFORE the iterator record is first examined
	void getNext ();

	// return true iff there is another record in the file/page
	virtual bool hasNext ();

	// destructor and contructor
	virtual ~MyDB_PageRecIterator () {};

private:
	
	MyDB_PageHandle myPage;
	MyDB_RecordPtr myRec;
	size_t currentOffset;
};

#endif


#ifndef TABLE_REC_ITER_H
#define TABLE_REC_ITER_H

#include "MyDB_RecordIterator.h"
#include "MyDB_TableReaderWriter.h"

class MyDB_TableRecIterator : public MyDB_RecordIterator {

public:

	// constructor
	MyDB_TableRecIterator (MyDB_TableReaderWriter &myParent, MyDB_TablePtr myTable, MyDB_RecordPtr myRec);

	// put the contents of the next record in the file/page into the iterator record
	// this should be called BEFORE the iterator record is first examined
	void getNext ();

	// return true iff there is another record in the file/page
	virtual bool hasNext ();

	// destructor and contructor
	virtual ~MyDB_TableRecIterator () {};

private:

	MyDB_TableReaderWriter &myParent;
	MyDB_TablePtr myTable;
	MyDB_RecordPtr myRec;
	MyDB_RecordIteratorPtr myPageIter;
	int curPage;
};

#endif

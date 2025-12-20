
#include "MyDB_TableRecIterator.h"
#include "MyDB_PageReaderWriter.h"

void MyDB_TableRecIterator :: getNext () {
	// Ensure we have a valid page iterator pointing to the next record
	if (hasNext()) {
		myPageIter->getNext();
	}
}

bool MyDB_TableRecIterator :: hasNext () {
	// If we have a current page iterator and it has more records, we are good
	if (myPageIter != nullptr && myPageIter->hasNext()) {
		return true;
	}

	// Otherwise, we need to search for the next page that has records
	while (curPage < myTable->lastPage()) {
		curPage++;
		// std::cout << "TableRecIter: advancing to page " << curPage << " of " << myTable->lastPage() << std::endl;
		// We get the next page from the parent
		// Note: we can't store MyDB_PageReaderWriter in the class because it's not a pointer/ref
		// But we can get it temporarily to get the iterator
		myPageIter = myParent[curPage].getIterator(myRec);
		
		if (myPageIter->hasNext()) {
			return true;
		}
	}

	return false;
}

MyDB_TableRecIterator :: MyDB_TableRecIterator (MyDB_TableReaderWriter &myParent, MyDB_TablePtr myTable, MyDB_RecordPtr myRec) : myParent(myParent) {
	this->myTable = myTable;
	this->myRec = myRec;
	this->curPage = -1; // Start before the first page
	this->myPageIter = nullptr;
}

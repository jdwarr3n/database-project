
#include "MyDB_PageRecIterator.h"
#include "MyDB_PageType.h"

void MyDB_PageRecIterator :: getNext () {
	if (hasNext()) {
		void *base = myPage->getBytes();
		void *pos = (char *)base + currentOffset;
		void *nextPos = myRec->fromBinary(pos);
		currentOffset = (char *)nextPos - (char *)base;
	}
}

bool MyDB_PageRecIterator :: hasNext () {
	void *base = myPage->getBytes();
	// Access the bytesUsed value which is stored after the page type
	size_t *bytesUsedPtr = (size_t *)((char *)base + sizeof(MyDB_PageType));
	size_t bytesUsed = *bytesUsedPtr;
	return currentOffset < bytesUsed;
}

MyDB_PageRecIterator :: MyDB_PageRecIterator (MyDB_PageHandle myPage, MyDB_RecordPtr myRec) {
	this->myPage = myPage;
	this->myRec = myRec;
	// Records start after the header: Type + BytesUsed
	this->currentOffset = sizeof(MyDB_PageType) + sizeof(size_t);
}


#ifndef PAGE_RW_C
#define PAGE_RW_C

#include "MyDB_PageReaderWriter.h"
#include "MyDB_PageRecIterator.h"

void MyDB_PageReaderWriter :: clear () {
	// Set the type to RegularPage
	setType(MyDB_PageType::RegularPage);
	
	// Reset bytes used to just the header size
	size_t *bytesUsedPtr = (size_t *)((char *)myPage->getBytes() + sizeof(MyDB_PageType));
	*bytesUsedPtr = sizeof(MyDB_PageType) + sizeof(size_t);
	
	myPage->wroteBytes();
}

MyDB_PageType MyDB_PageReaderWriter :: getType () {
	return *((MyDB_PageType *)myPage->getBytes());
}

MyDB_RecordIteratorPtr MyDB_PageReaderWriter :: getIterator (MyDB_RecordPtr iterateIntoMe) {
	return make_shared<MyDB_PageRecIterator>(myPage, iterateIntoMe);
}

void MyDB_PageReaderWriter :: setType (MyDB_PageType toMe) {
	*((MyDB_PageType *)myPage->getBytes()) = toMe;
	myPage->wroteBytes();
}

bool MyDB_PageReaderWriter :: append (MyDB_RecordPtr appendMe) {
	size_t *bytesUsedPtr = (size_t *)((char *)myPage->getBytes() + sizeof(MyDB_PageType));
	size_t bytesUsed = *bytesUsedPtr;
	size_t recSize = appendMe->getBinarySize();
	
	// Check if there is enough space
	if (bytesUsed + recSize > pageSize) {
		return false;
	}
	
	// Write the record
	void *dest = (char *)myPage->getBytes() + bytesUsed;
	appendMe->toBinary(dest);
	
	// Update bytes used
	*bytesUsedPtr = bytesUsed + recSize;
	myPage->wroteBytes();
	
	return true;
}

MyDB_PageReaderWriter :: MyDB_PageReaderWriter(MyDB_PageHandle myPage, size_t pageSize) {
	this->myPage = myPage;
	this->pageSize = pageSize;
}

#endif



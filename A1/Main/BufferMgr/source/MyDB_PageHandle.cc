
#include <memory>
#include "MyDB_PageHandle.h"

void *MyDB_PageHandleBase :: getBytes () {
	return page->getBytes();
}

void MyDB_PageHandleBase :: wroteBytes () {
	page->wroteBytes();
}

MyDB_PageHandleBase :: ~MyDB_PageHandleBase () {
    if (page) { // Check if valid
        page->decrementRefCount();
        if (page->getRefCount() == 0) {
            page->setUnpinned();
        }
    }
}

void MyDB_PageHandleBase :: setPage (MyDB_PagePtr pagePtr) {
    page = pagePtr;
    page->incrementRefCount(); // Increment when handle is attached
}


#include "MyDB_Page.h"
#include "MyDB_BufferManager.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef O_FSYNC
#define O_FSYNC 0
#endif

using namespace std;

void *MyDB_Page :: getBytes () {
    if (bytes == nullptr) {
        parent->repin(this);
    }
	return bytes;
}

void MyDB_Page :: wroteBytes () {
	isDirty = true;
}

MyDB_Page :: MyDB_Page (MyDB_TablePtr myTableIn, long i, void *bytesIn, bool pinned, size_t pageSizeIn, MyDB_BufferManager *parentIn) {
	myTable = myTableIn;
	pos = i;
	bytes = bytesIn;
	isDirty = false;
	isPinned = pinned;
	refCount = 0;
    pageSize = pageSizeIn;
    parent = parentIn;
}

MyDB_Page :: ~MyDB_Page () {
    // If we are an anonymous page, we need to recycle our slot in the temp file
    // The requirement says: "When all of the handles to an anonymous page are gone... disk slot recycled"
    if (parent && myTable == parent->tempTable) {
        parent->freeTempSlots.insert(pos);
    }
}

void MyDB_Page :: writeBack () {
	
	// if we are dirty, write back
	if (isDirty) {
		int fd = open (myTable->getStorageLoc ().c_str (), O_CREAT | O_RDWR | O_FSYNC, 0666);
		if (fd < 0) {
            // Error handling?
            cerr << "Error opening file: " << myTable->getStorageLoc() << endl;
            return;
        }
        
        lseek (fd, pos * pageSize, SEEK_SET);
        write (fd, bytes, pageSize);
        close (fd);
        
        isDirty = false;
	}
}

void MyDB_Page :: incrementRefCount () {
    refCount++;
}

void MyDB_Page :: decrementRefCount () {
    refCount--;
    if (refCount == 0 && parent && myTable == parent->tempTable) {
        parent->killPage(this);
    }
}

int MyDB_Page :: getRefCount () {
    return refCount;
}

bool MyDB_Page :: isPinnedPtr () {
    return isPinned;
}

bool MyDB_Page :: isDirtyPtr () {
    return isDirty;
}

void MyDB_Page :: setUnpinned () {
    isPinned = false;
}

MyDB_TablePtr MyDB_Page :: getTable () {
    return myTable;
}

long MyDB_Page :: getPos () {
    return pos;
}

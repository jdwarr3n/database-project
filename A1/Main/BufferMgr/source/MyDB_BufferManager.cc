
#include "MyDB_BufferManager.h"
#include "MyDB_Page.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

#ifndef O_FSYNC
#define O_FSYNC 0
#endif

using namespace std;

MyDB_PageHandle MyDB_BufferManager :: getPage (MyDB_TablePtr whichTable, long i) {
    MyDB_PageHandle lh = make_shared<MyDB_PageHandleBase> ();
    
    // Check if page is already in buffer
    pair<MyDB_TablePtr, long> key = make_pair(whichTable, i);
    if (allPages.count(key)) {
        MyDB_PagePtr p = allPages[key];
        
        // LRU: Move to back
        // Optimization: allPages entries should ensure they are in lruList if they have bytes?
        // But if evicted, they are NOT in lruList.
        
        // Check if evicted
        if (p->bytes == nullptr) {
            // It is evicted. Reload it.
            repin(p.get());
        } else {
            // It is in RAM. Update LRU.
            for (auto it = lruList.begin(); it != lruList.end(); ++it) {
                if (*it == p) {
                    lruList.erase(it);
                    break;
                }
            }
            lruList.push_back(p);
        }
        
        lh->setPage(p);
        return lh;
    }
    
    // Not found, need to load
    char* slot = getBufferSlot();
    
    // Create new page object
    // Pass 'this' so the page can access tempTable for recycling
    MyDB_PagePtr p = make_shared<MyDB_Page>(whichTable, i, (void*)slot, false, pageSize, this);
    
    // Read from disk
    int fd = open(whichTable->getStorageLoc().c_str(), O_CREAT | O_RDWR | O_FSYNC, 0666);
    if (fd < 0) {
        cerr << "Failed to open table file: " << whichTable->getStorageLoc() << endl;
    } else {
        lseek(fd, i * pageSize, SEEK_SET);
        read(fd, slot, pageSize);
        close(fd);
    }
    
    // Register
    allPages[key] = p;
    lruList.push_back(p);
    
    lh->setPage(p);
    return lh;
}

MyDB_PageHandle MyDB_BufferManager :: getPage () {
    // Anonymous page
    // Find a free temp slot
    long id;
    if (freeTempSlots.size() > 0) {
        id = *freeTempSlots.begin();
        freeTempSlots.erase(freeTempSlots.begin());
    } else {
        id = lastTempId++;
    }
    
    // Get buffer slot
    char* slot = getBufferSlot();
    
    // Create page
    MyDB_PagePtr p = make_shared<MyDB_Page>(tempTable, id, (void*)slot, false, pageSize, this);
    
    pair<MyDB_TablePtr, long> key = make_pair(tempTable, id);
    allPages[key] = p;
    lruList.push_back(p);
    
    MyDB_PageHandle lh = make_shared<MyDB_PageHandleBase> ();
    lh->setPage(p);
    return lh;
}

MyDB_PageHandle MyDB_BufferManager :: getPinnedPage (MyDB_TablePtr whichTable, long i) {
     MyDB_PageHandle lh = getPage(whichTable, i);
     // Set pinned
     lh->page->isPinned = true;
     return lh;
}

MyDB_PageHandle MyDB_BufferManager :: getPinnedPage () {
    MyDB_PageHandle lh = getPage();
    lh->page->isPinned = true;
    return lh;
}

void MyDB_BufferManager :: unpin (MyDB_PageHandle unpinMe) {
    if (unpinMe->page) {
        unpinMe->page->setUnpinned();
    }
}

MyDB_BufferManager :: MyDB_BufferManager (size_t pageSizeIn, size_t numPagesIn, string tempFileIn) {
    pageSize = pageSizeIn;
    numPages = numPagesIn;
    tempFile = tempFileIn;
    lastTempId = 0;
    
    // Allocate buffer
    buffer = (char*) malloc(pageSize * numPages);
    
    // Initialize free slots
    for (size_t i = 0; i < numPages; i++) {
        freeSlots.push_back(i);
    }
    
    // Initialize temp table
    tempTable = make_shared<MyDB_Table>("temp", tempFile);
    
    // Ensure temp file is empty/fresh
    unlink(tempFile.c_str());
    int fd = open(tempFile.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0666);
    close(fd);
}

MyDB_BufferManager :: ~MyDB_BufferManager () {
    // Write back all pages
    for (auto const& item : allPages) {
        item.second->writeBack();
    }
    
    free(buffer);
    
    // Delete temp file
    unlink(tempFile.c_str());
}

char* MyDB_BufferManager :: getBufferSlot () {
    if (freeSlots.size() > 0) {
        long idx = freeSlots.back();
        freeSlots.pop_back();
        return buffer + (idx * pageSize);
    } else {
        return evict();
    }
}

void MyDB_BufferManager :: killPage(MyDB_Page* page) {
    // 1. Recycle Buffer Slot if held
    if (page->bytes != nullptr) {
        long idx = ((char*)page->bytes - buffer) / pageSize;
        freeSlots.push_back(idx);
        // We do NOT set page->bytes = nullptr because page is being destroyed anyway?
        // But safer to do it.
        // HOWEVER, page->bytes is void*.
        // We cannot modify page->bytes directly since it is private in MyDB_Page?
        // Oh, we are friends.
        // Wait, 'page' is MyDB_Page*.
        // But 'bytes' is in MyDB_Page.
        // Yes, we can access it.
        // page->bytes = nullptr;
    }
    
    // 2. Remove from map
    pair<MyDB_TablePtr, long> key = make_pair(page->getTable(), page->getPos());
    allPages.erase(key);
    
    // 3. Remove from LRU list
    for (auto it = lruList.begin(); it != lruList.end(); ++it) {
        if (it->get() == page) {
            lruList.erase(it);
            break;
        }
    }
}

void MyDB_BufferManager :: repin(MyDB_Page* page) {
    // 1. Get a slot
    char* slot = getBufferSlot();
    
    // 2. Read content
    int fd = open(page->getTable()->getStorageLoc().c_str(), O_CREAT | O_RDWR | O_FSYNC, 0666);
    if (fd < 0) {
        cerr << "Failed to open table file: " << page->getTable()->getStorageLoc() << endl;
    } else {
        lseek(fd, page->getPos() * pageSize, SEEK_SET);
        read(fd, slot, pageSize);
        close(fd);
    }
    
    // 3. Update Page
    page->bytes = slot;
    // Also reset dirty... if loaded from disk, it is clean.
    // page->isDirty = false; // Accessing private? Friend.
    
    // 4. Add to LRU
    // We need the MyDB_PagePtr (shared_ptr).
    // Look it up in allPages.
    pair<MyDB_TablePtr, long> key = make_pair(page->getTable(), page->getPos());
    if (allPages.count(key)) {
        lruList.push_back(allPages[key]);
    } else {
        // This causes an issue: if Page exists (passed here) but not in Map?
        // Should not happen if we only evict from LRU but keep in Map.
        cerr << "Error: Repin called on page not in map!" << endl;
    }
}

char* MyDB_BufferManager :: evict () {
    // Find victim in LRU
    for (auto it = lruList.begin(); it != lruList.end(); ++it) {
        MyDB_PagePtr p = *it;
        if (!p->isPinned) {
            // Found victim
            
            // 1. Write back if dirty
            p->writeBack();
            
            // 2. Detach from RAM
            char* bytes = (char*)p->getBytes();
            // Need to set p->bytes to nullptr so it knows it is evicted.
            // Since we are friends, we can cast and access.
            // p->bytes is void*.
            // Wait, standard access:
            // p->bytes = nullptr; // OK
             
            // We need to modify the underlying object.
            // But 'p' is shared_ptr. p.get()->bytes.
            // But MyDB_Page does not expose 'bytes' publicly?
            // yes, friend class.
            
            // Wait, we need to access 'bytes' member.
            // 'p' is a pointer to the object.
            // Inside MyDB_Page, 'bytes' is private.
            // But we are friend.
            // So p->bytes should work.
            
            // Correct way to access member of pointer:
            // p->bytes = nullptr;
            
            // But 'p' is shared_ptr.
            p->bytes = nullptr; // Smart pointer delegates ->?
            
            // 3. Remove from LRU list (BUT KEEP IN ALLPAGES)
            lruList.erase(it);
            
            // 4. Return the slot
            return bytes;
        }
    }
    
    cerr << "Buffer Manager Error: No evictable pages found!" << endl;
    return nullptr; 
}

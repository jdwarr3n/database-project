
#include "MyDB_BufferManager.h"
#include "MyDB_Page.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

#ifndef O_FSYNC
#define O_FSYNC 0
#endif

using namespace std;

void MyDB_BufferManager :: updateLRU(MyDB_PagePtr p) {
    // If pinned, do NOT add to LRU
    if (p->isPinnedPtr()) {
        removeFromLRU(p.get());
        return;
    }

    // Check if already in LRU
    auto it = pageLruIterators.find(p.get());
    if (it != pageLruIterators.end()) {
        // Remove existing
        lruList.erase(it->second);
    }

    // Push to back (MRU)
    lruList.push_back(p);
    
    // Update iterator
    // "std::list::end()" is not stable if we push back? 
    // Wait, push_back validation.
    // list iterators are stable.
    auto lastIt = lruList.end();
    lastIt--; // point to the element just added
    pageLruIterators[p.get()] = lastIt;
}

void MyDB_BufferManager :: removeFromLRU(MyDB_Page* p) {
    auto it = pageLruIterators.find(p);
    if (it != pageLruIterators.end()) {
        lruList.erase(it->second);
        pageLruIterators.erase(it);
    }
}

MyDB_PageHandle MyDB_BufferManager :: getPage (MyDB_TablePtr whichTable, long i) {
    MyDB_PageHandle lh = make_shared<MyDB_PageHandleBase> ();
    
    // O(1) Lookup
    pair<MyDB_TablePtr, long> key = make_pair(whichTable, i);
    auto it = allPages.find(key);
    
    if (it != allPages.end()) {
        MyDB_PagePtr p = it->second;

        // Check if evicted
        if (p->bytes == nullptr) {
            // Reload
            repin(p.get());
        } 
        
        // Update LRU (Move to MRU, or ensure it's removed if pinned)
        // Note: getPage() usually doesn't pin. But the page *might* be pinned if someone else called getPinnedPage().
        // If it's pinned, updateLRU handles explicitly not adding it.
        updateLRU(p);
        
        lh->setPage(p);
        return lh;
    }
    
    // Not found, load it
    char* slot = getBufferSlot();
    MyDB_PagePtr p = make_shared<MyDB_Page>(whichTable, i, (void*)slot, false, pageSize, this);
    
    // Load data
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
    // Add to LRU (since it is unpinned by default)
    updateLRU(p);
    
    lh->setPage(p);
    return lh;
}

MyDB_PageHandle MyDB_BufferManager :: getPage () {
    // Anonymous
    long id;
    if (freeTempSlots.size() > 0) {
        id = *freeTempSlots.begin();
        freeTempSlots.erase(freeTempSlots.begin());
    } else {
        id = lastTempId++;
    }
    
    char* slot = getBufferSlot();
    MyDB_PagePtr p = make_shared<MyDB_Page>(tempTable, id, (void*)slot, false, pageSize, this);
    
    pair<MyDB_TablePtr, long> key = make_pair(tempTable, id);
    allPages[key] = p;
    updateLRU(p);
    
    MyDB_PageHandle lh = make_shared<MyDB_PageHandleBase> ();
    lh->setPage(p);
    return lh;
}

MyDB_PageHandle MyDB_BufferManager :: getPinnedPage (MyDB_TablePtr whichTable, long i) {
     MyDB_PageHandle lh = getPage(whichTable, i);
     // Set pinned
     lh->page->isPinned = true;
     // Remove from LRU (Instructor Requirement)
     removeFromLRU(lh->page.get());
     return lh;
}

MyDB_PageHandle MyDB_BufferManager :: getPinnedPage () {
    MyDB_PageHandle lh = getPage();
    lh->page->isPinned = true;
    removeFromLRU(lh->page.get());
    return lh;
}

void MyDB_BufferManager :: unpin (MyDB_PageHandle unpinMe) {
    if (unpinMe->page) {
        unpinMe->page->setUnpinned();
        // Now valid for LRU
        updateLRU(unpinMe->page); 
    }
}

MyDB_BufferManager :: MyDB_BufferManager (size_t pageSizeIn, size_t numPagesIn, string tempFileIn) {
    pageSize = pageSizeIn;
    numPages = numPagesIn;
    tempFile = tempFileIn;
    lastTempId = 0;
    
    buffer = (char*) malloc(pageSize * numPages);
    
    for (size_t i = 0; i < numPages; i++) {
        freeSlots.push_back(i);
    }
    
    tempTable = make_shared<MyDB_Table>("temp", tempFile);
    
    unlink(tempFile.c_str());
    int fd = open(tempFile.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0666);
    close(fd);
}

MyDB_BufferManager :: ~MyDB_BufferManager () {
    for (auto const& item : allPages) {
        item.second->writeBack();
    }
    free(buffer);
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
    // 1. Recycle Buffer Slot
    if (page->bytes != nullptr) {
        long idx = ((char*)page->bytes - buffer) / pageSize;
        freeSlots.push_back(idx);
        page->bytes = nullptr; // Safety
    }
    
    // 2. Remove from map
    pair<MyDB_TablePtr, long> key = make_pair(page->getTable(), page->getPos());
    allPages.erase(key);
    
    // 3. Remove from LRU (O(1))
    removeFromLRU(page);
}

void MyDB_BufferManager :: repin(MyDB_Page* page) {
    char* slot = getBufferSlot();
    
    int fd = open(page->getTable()->getStorageLoc().c_str(), O_CREAT | O_RDWR | O_FSYNC, 0666);
    if (fd < 0) {
        cerr << "Failed to open table file: " << page->getTable()->getStorageLoc() << endl;
    } else {
        lseek(fd, page->getPos() * pageSize, SEEK_SET);
        read(fd, slot, pageSize);
        close(fd);
    }
    
    page->bytes = slot;
    
    // Ensure accurate LRU stat
    // But repin takes raw pointer. We need Ptr to updateLRU.
    // Lookup in allPages to get the smart pointer.
    pair<MyDB_TablePtr, long> key = make_pair(page->getTable(), page->getPos());
    auto it = allPages.find(key);
    if (it != allPages.end()) {
        updateLRU(it->second);
    }
}

char* MyDB_BufferManager :: evict () {
    // O(1) Eviction: victim is always front of lruList
    if (lruList.empty()) {
        cerr << "Buffer Manager Error: No evictable pages found!" << endl;
        return nullptr;
    }
    
    MyDB_PagePtr victim = lruList.front();
    
    // Verify NOT pinned (Should be guaranteed by logic, but check safety)
    if (victim->isPinned) {
        // This means logic failed somewhere.
        // Recover: remove from list and try again.
        lruList.pop_front();
        pageLruIterators.erase(victim.get());
        return evict();
    }
    
    // 1. Write back
    victim->writeBack();
    
    // 2. Detach
    char* bytes = (char*)victim->getBytes();
    victim->bytes = nullptr; // Mark evicted
    
    // 3. Remove from LRU
    lruList.pop_front();
    pageLruIterators.erase(victim.get());
    
    // 4. Return slot
    return bytes;
}

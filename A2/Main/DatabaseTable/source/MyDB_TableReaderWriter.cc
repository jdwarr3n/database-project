
#ifndef TABLE_RW_C
#define TABLE_RW_C

#include <fstream>
#include <sstream>
#include "MyDB_PageReaderWriter.h"
#include "MyDB_TableReaderWriter.h"
#include "MyDB_TableRecIterator.h"

using namespace std;

MyDB_TableReaderWriter :: MyDB_TableReaderWriter (MyDB_TablePtr forMe, MyDB_BufferManagerPtr myBuffer) {
	this->myTable = forMe;
	this->myBuffer = myBuffer;
	
	// If the table is empty (lastPage is -1), initialize it with page 0
	if (myTable->lastPage() == -1) {
		myTable->setLastPage(0);
		(*this)[0].clear();
	}
}

MyDB_PageReaderWriter MyDB_TableReaderWriter :: operator [] (size_t i) {
	MyDB_PageHandle myHandle = myBuffer->getPage(myTable, i);
	return MyDB_PageReaderWriter(myHandle, myBuffer->getPageSize());
}

MyDB_RecordPtr MyDB_TableReaderWriter :: getEmptyRecord () {
	return make_shared<MyDB_Record>(myTable->getSchema());
}

MyDB_PageReaderWriter MyDB_TableReaderWriter :: last () {
	return (*this)[myTable->lastPage()];
}

void MyDB_TableReaderWriter :: append (MyDB_RecordPtr appendMe) {
	// Try appending to the last page
	if (last().append(appendMe)) {
		return;
	}
	
	// If failed, create a new page
	myTable->setLastPage(myTable->lastPage() + 1);
	// std::cout << "DEBUG: Full page, creating page " << myTable->lastPage() << std::endl; 
	MyDB_PageReaderWriter newPage = last();
	newPage.clear();
	if (newPage.append(appendMe) == false) {
		// std::cout << "DEBUG: Record size " << appendMe->getBinarySize() << " too large for page size " << myBuffer->getPageSize() << std::endl;
	}
}

void MyDB_TableReaderWriter :: loadFromTextFile (string fromMe) {
	// Clear the table
	myTable->setLastPage(0);
	(*this)[0].clear();
	
	MyDB_RecordPtr record = getEmptyRecord();
	
	ifstream infile(fromMe);
	string line;
	while (getline(infile, line)) {
		record->fromString(line);
		append(record);
	}
}

MyDB_RecordIteratorPtr MyDB_TableReaderWriter :: getIterator (MyDB_RecordPtr iterateIntoMe) {
	return make_shared<MyDB_TableRecIterator>(*this, myTable, iterateIntoMe);
}

void MyDB_TableReaderWriter :: writeIntoTextFile (string toMe) {
	ofstream outfile(toMe);
	MyDB_RecordPtr record = getEmptyRecord();
	MyDB_RecordIteratorPtr iter = getIterator(record);
	
	while (iter->hasNext()) {
		iter->getNext();
		outfile << record << "\n";
	}
}

#endif




// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#define FILESYSTEM_JOURNAL_CPP
#include "journal.h"
#include "storage.h"
#include "fs.h"
#include "modules/util/files.h"
#include <algorithm>
#include <filesystem>
#include <thread>
#include <fstream>

using namespace filesystem;


journalEntryWrapper::journalEntryWrapper(
	const uint32_t iid,
	const journalEntryType itype, 
	const bucketIndex_t iparentNode,
	const bucketIndex_t inewNode,
	const bucketIndex_t inewParentNode,
	const my_mode_t imod,
	const my_off_t ioffset, 
	const str & name, 
	const str & data
) : 
	inner{iid,itype,iparentNode,inewNode,inewParentNode,imod,ioffset,name.size(),data.size()}
	{
	file = JOURNAL->writeEntry(&inner,name,data);
	_ASSERT(file!=nullptr);
}

journalEntryWrapper::~journalEntryWrapper() {
	file->deleteEntry(&inner);
}

namespace filesystem {
	class journalFileImpl{
		public:
		std::ofstream F;
		shared_ptr<crypto::streamInterface> cryptostream;
	};
};


journalFile::journalFile(const str & ifilename) : filename(ifilename),entries(0), impl(std::make_unique<journalFileImpl>()) {
	impl->F.open(filename.c_str(),std::ios::binary|std::ios::out|std::ios::app);
	str header;
	impl->cryptostream = STOR->prot()->startStreamWrite(STOR->prot()->getProtoEncryptionKey(),header);
	JOURNAL->srvMESSAGE("creating log: ",filename);
	impl->F.write(header.data(),header.size());
}

journalFile::~journalFile() {
	if(impl->F.is_open()) {
		impl->F.close();
	}
	JOURNAL->srvMESSAGE("removing log: ",filename);
	_ASSERT(std::filesystem::remove(filename.c_str())==true);
}

void journalFile::writeEntry(const journalEntry * entry,const str & name,const str & data) {
	_ASSERT(impl->F.is_open());
	const char * ep = reinterpret_cast<const char *>(entry);
	str content(ep,sizeof(journalEntry));
	_ASSERT(content.size()==sizeof(journalEntry));
	str datacontent(name);
	datacontent.append(data);
	_ASSERT(datacontent.size()==name.size()+data.size());
	str encryptedContent;
	impl->cryptostream->message(content,encryptedContent);
	if(datacontent.size()) {
		str encryptedData;
		impl->cryptostream->message(datacontent,encryptedData);
		encryptedContent.append(encryptedData);
	}
	JOURNAL->srvDEBUG(entry->type==journalEntryType::close? "Removing": "Adding"," journal entry ",entry->id," size: ",encryptedContent.size()," log: ",filename);
	impl->F.write(encryptedContent.data(),encryptedContent.size());
	impl->F.flush();
	++entries;
}

void journalFile::deleteEntry(const journalEntry * entry) {
	const journalEntry closeEntry{entry->id,journalEntryType::close,bucketIndex_t(),bucketIndex_t(),bucketIndex_t(),0,0,0,0};
	writeEntry(&closeEntry,"","");
}




shared_ptr<journalFile> journal::getJournalFile() {
	static thread_local std::weak_ptr<journalFile> jf;
	
	if(auto myFile = jf.lock()) {
		if(myFile->getEntries() > 1024) {// if too many entries for file, make new file & return that.
			size_t nextid = nextJournalEntry.fetch_add(1);
			const str filename = BUILDSTRING(STOR->getPath(),"journal/.",nextid);
			myFile = std::make_shared<journalFile>(filename);
			jf = myFile;
		}
		
		return myFile;
	}
	size_t id = nextJournalEntry.fetch_add(1);
	
	const str filename = BUILDSTRING(STOR->getPath(),"journal/.",id);
	
	auto ret = std::make_shared<journalFile>(filename);
	
	jf = ret;
	
	return ret;
}


shared_ptr<journalFile> journal::writeEntry(const journalEntry * entry,const str & name,const str & data) {
	auto F = getJournalFile();
	
	F->writeEntry(entry,name,data);
	
	return F;
}

void journal::tryReplay(void) {
	auto ptr = make_unique<filesystem::context>(); 
	std::vector<str> files;
	str completeJournal;
	for (const auto & entry : std::filesystem::directory_iterator(path.c_str())) { 
		const str p = str(entry.path().generic_string().c_str());
		str encryptedJournal = util::getSystemString(p);
		str header = encryptedJournal.substr(0,STOR->prot()->streamHeaderSize());
		auto cryptostream = STOR->prot()->startStreamRead(STOR->prot()->getProtoEncryptionKey(),header);
		size_t offset = STOR->prot()->streamHeaderSize();
		auto entryEncSize = cryptostream->encryptionOverhead(sizeof(journalEntry));
		while(offset<encryptedJournal.size()) {
			str entryEnc(&encryptedJournal[offset],entryEncSize);
			str entry;
			cryptostream->message(entryEnc,entry);
			_ASSERT(entry.size()==sizeof(journalEntry));
			completeJournal.append(entry);
			auto e = reinterpret_cast<const journalEntry *>(&entry[0]);
			offset+=entryEncSize;
			if(e->dataLength + e->nameLength) {
				auto dataEncSize = cryptostream->encryptionOverhead(e->nameLength+e->dataLength);
				str dataEnc(&encryptedJournal[offset],dataEncSize);
				str data;
				cryptostream->message(dataEnc,data);
				completeJournal.append(data);
				offset+=dataEncSize;
			}
		}
		files.push_back(p);
	}
	struct _item{
		size_t offset,size;
	};
	//make damn sure the entries are in order before replay!
	std::map<uint64_t,_item> items;
	size_t offset = 0;
	
	while(offset<completeJournal.size()) {
		auto entry = reinterpret_cast<const journalEntry *>(&completeJournal[offset]);
		switch(entry->type) {
			case journalEntryType::close:
				items.erase(entry->id);
				break;
			case journalEntryType::mkobject:
			case journalEntryType::write:
			case journalEntryType::unlink:
			case journalEntryType::renamemove:
			case journalEntryType::chown:
			case journalEntryType::chmod:
			case journalEntryType::truncate:
				items[entry->id].offset = offset;
				items[entry->id].size = sizeof(journalEntry)+entry->nameLength+entry->dataLength;
				break;
			default:
				throw std::logic_error("corrupted journal");
				break;
		}
		offset+= sizeof(journalEntry)+entry->nameLength+entry->dataLength;
	}
	
	//Items should be a sorted list of journal items. @todo: when the id wraps around this could cause problems.	
	if(items.empty()==false) {
		srvWARNING(items.size()," journal entries found. Replaying...");
	}
	
	for(auto & i: items) {
		str content(&completeJournal[i.second.offset],i.second.size);
		if(content.size()>=sizeof(journalEntry)) {
			auto entry = reinterpret_cast<const journalEntry *>(content.data());
			if(content.size()>= sizeof(journalEntry)+entry->nameLength+entry->dataLength) {
				
				str name(content.data()+sizeof(journalEntry),entry->nameLength);
				str data(content.data()+sizeof(journalEntry)+entry->nameLength,entry->dataLength);
				
				auto e = FS->replayEntry(entry,name,data,ptr.get(),nullptr);
				if(e) {
					srvERROR("Journal entry ",i.first," replay failed with error: ",e.operator int());
				} else {
					srvMESSAGE("Replayed journal entry ",i.first);
				}
			}
		} else {
			srvERROR("Journal entry failed to load: ",i.first);
		}
	}
	
	for(auto & f:files) {
		std::filesystem::remove(f);
	}
	
}

journal::journal():service("JOURNAL"), nextJournalEntry(0) {
	path = STOR->getPath()+"journal";
	
	util::MKDIR(path);
	srvMESSAGE("journaling directory: ",path);
	
}


journal::~journal() {

}

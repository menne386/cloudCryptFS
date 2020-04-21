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
	};
};


journalFile::journalFile(size_t iid,const str & ifilename) : id(iid),filename(ifilename),entries(0), impl(std::make_unique<journalFileImpl>()) {
	impl->F.open(filename.c_str(),std::ios::binary|std::ios::out|std::ios::app);
	JOURNAL->srvMESSAGE("creating log: ",filename);
}

journalFile::~journalFile() {
	if(impl->F.is_open()) {
		impl->F.close();
	}
	JOURNAL->srvMESSAGE("removing log: ",filename);
	_ASSERT(std::filesystem::remove(filename.c_str())==true);
}

void journalFile::writeEntry(const journalEntry * entry,const str & name,const str & data) {
	const char * ep = reinterpret_cast<const char *>(entry);
	str content(ep,sizeof(journalEntry));
	content.append(name);
	content.append(data);
	_ASSERT(content.size()==sizeof(journalEntry)+name.size()+data.size());
	JOURNAL->srvDEBUG(entry->type==journalEntryType::close? "Removing": "Adding"," journal entry ",entry->id," size: ",content.size()," log: ",filename);
	_ASSERT(impl->F.is_open());
	impl->F.write(content.data(),content.size());
	++entries;
}

void journalFile::deleteEntry(const journalEntry * entry) {
	const journalEntry closeEntry{entry->id,journalEntryType::close,bucketIndex_t(),bucketIndex_t(),bucketIndex_t(),0,0,0,0};
	writeEntry(&closeEntry,"","");
}




shared_ptr<journalFile> journal::getJournalFile() {
	std::hash<std::thread::id> hasher;
	static thread_local std::weak_ptr<journalFile> jf;
	
	if(auto myFile = jf.lock()) {
		if(myFile->getEntries() > 1024) {// if too many entries for file, make new file & return that.
			size_t nextid = myFile->getId() + 1;
			auto this_id = std::this_thread::get_id();
			const str filename = BUILDSTRING(STOR->getPath(),"journal/",hasher(this_id),".",nextid);
			myFile = std::make_shared<journalFile>(nextid,filename);
			jf = myFile;
		}
		
		return myFile;
	}
	size_t id = 0;
	auto this_id = std::this_thread::get_id();
	
	const str filename = BUILDSTRING(STOR->getPath(),"journal/",hasher(this_id),".",id);
	
	auto ret = std::make_shared<journalFile>(id,filename);
	
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
		const str p = entry.path().c_str();
		completeJournal.append(util::getSystemString(p));
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

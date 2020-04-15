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

using namespace filesystem;


journalEntry::journalEntry(
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
	id(iid), 
	type(itype), 
	parentNode(iparentNode), 
	newNode(inewNode), 
	newParentNode(inewParentNode),
	mod(imod),
	offset(ioffset),
	nameLength(name.size()),
	dataLength(data.size())
	{
	JOURNAL->writeEntry(this,name,data);
}

journalEntry::~journalEntry() {
	JOURNAL->deleteEntry(this);
}



void journal::writeEntry(const journalEntry * entry,const str & name,const str & data) {
	const char * ep = reinterpret_cast<const char *>(entry);
	
	str content(ep,sizeof(journalEntry));
	content.append(name);
	content.append(data);
	_ASSERT(content.size()==sizeof(journalEntry)+name.size()+data.size());
	JOURNAL->srvDEBUG("Adding journal entry ",entry->id," size: ",content.size());
	_ASSERT(util::putSystemString(STOR->getPath()+"journal/"+std::to_string(entry->id).c_str(),content)==true);
}


void journal::deleteEntry(const journalEntry * entry) {
	JOURNAL->srvDEBUG("Remove journal entry ",entry->id);
	const str filename = STOR->getPath()+"journal/"+std::to_string(entry->id).c_str();
	
	_ASSERT(std::filesystem::remove(filename.c_str())==true);
}

void journal::tryReplay(void) {
	auto ptr = make_unique<filesystem::context>(); 
	//make damn sure the entries are in order before replay!
	std::map<uint64_t,std::filesystem::path> items;
	for (const auto & entry : std::filesystem::directory_iterator(path.c_str())) { 
		items[std::stoul(entry.path().filename())] = entry.path();
	}
	//Items should be a sorted list of journal items. @todo: when the id wraps around this could cause problems.
	if(items.empty()==false) {
		srvWARNING(items.size()," journal entries found. Replaying...");
	}
	for(auto & i: items) {
		str content = util::getSystemString(i.second.c_str());
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
					std::filesystem::remove(i.second);
				}
			}
		} else {
				srvERROR("Journal entry failed to load: ",i.first);
		}
	}
	
	
}

journal::journal():service("JOURNAL"), nextJournalEntry(0) {
	path = STOR->getPath()+"journal";
	
	util::MKDIR(path);
	srvMESSAGE("journaling directory: ",path);
	
}


journal::~journal() {

}

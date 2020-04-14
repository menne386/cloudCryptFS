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


journalEntry::journalEntry(const uint32_t iid,const journalEntryType itype, const bucketIndex_t iparentNode,const bucketIndex_t inewNode,const bucketIndex_t inewParentNode,const my_mode_t imod, const str & name, const str & data) 
	: id(iid), type(itype), parentNode(iparentNode), newNode(inewNode), newParentNode(inewParentNode),mod(imod),nameLength(name.size()),dataLength(data.size()) {
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
	for (const auto & entry : std::filesystem::directory_iterator(path.c_str())) { //@todo: make damn sure the entries are in order before replay!
		srvWARNING("Journal entry found: ", entry.path().c_str());
		str content = util::getSystemString(entry.path().c_str());
		if(content.size()>=sizeof(journalEntry)) {
			auto entry = reinterpret_cast<const journalEntry *>(content.data());
			if(content.size()>= sizeof(journalEntry)+entry->nameLength+entry->dataLength) {
				
				str name(content.data()+sizeof(journalEntry),entry->nameLength);
				str data(content.data()+sizeof(journalEntry)+entry->nameLength,entry->dataLength);
				
				auto e = FS->replayEntry(entry,name,data,ptr.get(),nullptr);
				if(e) {
					srvERROR("Journal entry replay failed with error: ",e.operator int());
				}
			}
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

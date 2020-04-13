// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#define FILESYSTEM_JOURNAL_CPP
#include "journal.h"
#include "storage.h"
#include "modules/util/files.h"
#include <algorithm>

using namespace filesystem;


journalEntry::journalEntry(const uint32_t iid,const journalEntryType itype, const bucketIndex_t iparentNode,const bucketIndex_t inewNode,const bucketIndex_t inewParentNode, const str & inewNodeName) 
	: id(iid), type(itype), parentNode(iparentNode), newNode(inewNode), newParentNode(inewParentNode) {
	_ASSERT(inewNodeName.size()<sizeof(newNodeName));
	std::fill(std::begin(newNodeName),std::end(newNodeName),0);
	std::copy(inewNodeName.begin(),inewNodeName.end(),std::begin(newNodeName));
	
	//util::putSystemString(STOR->getPath()+"journal/"+);
}

journalEntry::~journalEntry() {
	
}


journal::journal():service("JOURNAL"), nextJournalEntry(0) {
	util::MKDIR(STOR->getPath()+"journal");
}

journal::~journal() {

}

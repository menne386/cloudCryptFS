// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef FILESYSTEM_JOURNAL_H
#define FILESYSTEM_JOURNAL_H

#include "types.h"
#include "hash.h"
#include <atomic>
#include "modules/services/serviceHandler.h"

namespace filesystem {

	
	using services::service;

	enum class journalEntryType : uint32_t {
		mkobject=0x02,write=0x03,unlink=0x04
	};
	
	class journalEntry{
	public:
		const uint32_t id; //journal entry id
		const journalEntryType type; //The type of the change
		const bucketIndex_t parentNode; //if the change needs a parentNode
		const bucketIndex_t newNode; //The new node
		const bucketIndex_t newParentNode; // For when moving a node.
		const my_mode_t mod;
		char newNodeName[256]; //newNodeName
		
		journalEntry(const uint32_t iid,const journalEntryType itype, const bucketIndex_t iparentNode,const bucketIndex_t inewNode,const bucketIndex_t inewParentNode, const my_mode_t imod, const str & inewNodeName);
		~journalEntry();
		
	};
	
	typedef std::shared_ptr<journalEntry> journalEntryPtr;
	
/**
 */
class journal: public service {
private:
    std::atomic_uint32_t nextJournalEntry;
public:
    /**
     * Default constructor
     */
    journal();
		
    /**
     * Destructor
     */
    ~journal();
		
		template<class ...Args>
		journalEntryPtr add(Args... args) {
			return make_shared<journalEntry>(nextJournalEntry.fetch_add(1),args...);
		}

		srvSTATICDEFAULTNEWINSTANCE( journal );
};

}

#ifdef FILESYSTEM_JOURNAL_CPP
services::t_autoStartService<filesystem::journal> JOURNAL;
#else
extern services::t_autoStartService<filesystem::journal> JOURNAL;
#endif


#endif // FILESYSTEM_JOURNAL_H

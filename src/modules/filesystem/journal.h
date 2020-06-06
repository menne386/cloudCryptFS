// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef FILESYSTEM_JOURNAL_H
#define FILESYSTEM_JOURNAL_H

#include "types.h"
#include "hash.h"
#include <atomic>
#include <set>
#include "modules/services/serviceHandler.h"

namespace filesystem {

	
	using services::service;

	enum class journalEntryType : uint32_t {
		close=0x10,
		mkobject=0x20,
		write=0x30,
		unlink=0x40,
		renamemove=0x50
	};
	class journalFile;
	
	
	class journalEntry{
	public:
		const uint32_t id; //journal entry id
		const journalEntryType type; //The type of the change
		const bucketIndex_t parentNode; //if the change needs a parentNode
		const bucketIndex_t newNode; //The new node
		const bucketIndex_t newParentNode; // For when moving a node.
		const my_mode_t mod;
		const my_off_t offset;
		const my_size_t nameLength;
		const my_size_t dataLength;
		
	};
	
	class journalEntryWrapper{
		private:
		friend class journal;
	    const journalEntry inner;
	    shared_ptr<journalFile> file;
		public:
		
		const journalEntry * entry() { return &inner; }

		journalEntryWrapper(const uint32_t iid,const journalEntryType itype, const bucketIndex_t iparentNode,const bucketIndex_t inewNode,const bucketIndex_t inewParentNode,const my_mode_t imod, const my_off_t ioffset, const str & name="", const str & data="");
		~journalEntryWrapper();		
	};
	
	typedef std::shared_ptr<journalEntryWrapper> journalEntryPtr;
	
	
	class journalFileImpl;
	class journalFile{
		private:
		const str filename;
		unsigned entries;
		unique_ptr<journalFileImpl> impl;
		public:
		
		journalFile(const str & ifilename);
		~journalFile();

		unsigned getEntries() { return entries; }
		
		void writeEntry(const journalEntry * entry,const str & name,const str & data);
		void deleteEntry(const journalEntry * entry);
		
		
	};
	
/**
 */
class journal: public service {
private:
	std::atomic_uint32_t nextJournalEntry;
	str path;
	friend class journalEntryWrapper;
	shared_ptr<journalFile> getJournalFile();
	shared_ptr<journalFile> writeEntry(const journalEntry * entry,const str & name,const str & data);
	
	
	
	
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
		return  make_shared<journalEntryWrapper>(nextJournalEntry.fetch_add(1),args...);
	}
	
	void tryReplay(void);
	
	srvSTATICDEFAULTNEWINSTANCE( journal );
};

}

#ifdef FILESYSTEM_JOURNAL_CPP
services::t_autoStartService<filesystem::journal> JOURNAL;
#else
extern services::t_autoStartService<filesystem::journal> JOURNAL;
#endif


#endif // FILESYSTEM_JOURNAL_H

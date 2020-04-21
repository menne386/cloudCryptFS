// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef _FS_H
#define _FS_H
#include <atomic>
#include <set>
#include <unordered_map>
#include <deque>

#include "modules/services/serviceHandler.h"
#include "modules/script/JSON.h"
#include "modules/crypto/key.h"
#include "modules/crypto/protocol.h"
#include "modules/crypto/sha256.h"
#include "modules/util/protected_unordered_map.h"
#include "locks.h"
#include "file.h"
#include "hash.h"
#include "context.h"


namespace filesystem{

	using services::service;

	enum class renOpt{
		NORMAL,EXCHANGE,REPLACE
	};
	
	class chunk;
	class bucket;
	class hashbucket;
	class bucketaccounting;
	class trackedChange;
	typedef std::shared_ptr<chunk> metaPtr;
	typedef uint64_t fileHandle;
	class fs : public service {
	private:
		friend class trackedChange;
		friend class bucketInfo;
		friend class file;
		friend class hash;
		
		
		filePtr root; //This contains the 1st bootstrap inode.
		
		static constexpr unsigned maxOpenFiles = 128;
		locktype _mut;
		util::protected_unordered_map<str,bucketIndex_t> pathInodeCache;
		util::protected_unordered_map<const bucketIndex_t,filePtr> inodeFileCache;
		std::array<util::atomic_shared_ptr<file>,maxOpenFiles> openHandles;
		
		metaPtr mkobject(const char * filename,my_err_t & errorcode,const context * ctx,my_mode_t type, my_mode_t mod);
		
		
		



		
		void storeMetadata(void);
		
		locktype outstandingWrites;
		locktype actualWriting;
	
	
		bool up_and_running = false;
		

		
		filePtr specialfile_error;
		
		std::atomic_int outstandingChanges;
		metaPtr zeroChunk;
		crypto::sha256sum zeroSum;
		hashPtr _zeroHash;
		std::array<std::atomic_uint64_t,5> _writeStats,_readStats;
		static unsigned classifySize(my_size_t size) {
			if(size<chunkSize) return 0;
			if(size==chunkSize) return 1;
			if(size<chunkSize * 9) return 2;
			if(size<chunkSize * chunksInBucket) return 3;
			return 4;
		}
		metaPtr createCtd(inode * prevNode, bool forMeta);
		metaPtr createCtd(inode_ctd * prevNode);
		void storeInode(metaPtr in);
		
	public:
		static constexpr bucketIndex_t rootIndex{1,0};
		static constexpr bucketIndex_t metaIndex{1,1};
		

		srvSTATICDEFAULTNEWINSTANCE( fs );
			fs();
			~fs();

		
		void migrate(unique_ptr<crypto::protocolInterface> newProtocol);


		my_err_t replayEntry(const journalEntry * entry, const str & name, const str & data,const context * ctx,shared_ptr<journalEntryWrapper> je);
		
		str loadMetaDataFromINode(inode* node);
		void storeMetaDataInINode(inode * rootNode,const str & input);
		bool initFileSystem ( unique_ptr<crypto::protocolInterface> iprot,bool mustCreate ) ;
		filePtr get(const char * filename,my_err_t * errcode = nullptr,const fileHandle H = 0);
		filePtr inodeToFile(const bucketIndex_t i,const char * filename,my_err_t * errcode,const std::vector<permission> & pPerm={});
		
		fileHandle open(filePtr F);
		void close(filePtr F,fileHandle H);

		my_err_t _mkdir(const char * name,my_mode_t mode, const context * ctx=nullptr);
		my_err_t mknod(const char * filename, my_mode_t mode, my_dev_t dev, const context * ctx=nullptr);
		my_err_t unlink(const char * filename, const context * ctx=nullptr);
		my_err_t renamemove(const char * source, const char * destination, const context * ctx=nullptr);
		my_err_t softlink(const char * linktarget,const char * linkname, const context * ctx=nullptr);
		my_err_t hardlink(const char * linktarget,const char * linkname, const context * ctx=nullptr);
		my_size_t evictCache(const char * ipath);
		
		void unloadBuckets(void);
		
		metaPtr inoToChunk(bucketIndex_t ino);
		void removeInode(bucketIndex_t ino);
		
		
		
		hashPtr zeroHash();
		
		str getStats(void);
	
		my_size_t metadataSize();
		my_off_t readMetadata(unsigned char * buf,my_size_t size, my_off_t offset);
		std::pair<uint64_t,uint64_t> getStatFS();
		
		static str getParentPath(const str & in);
		static str getChildPath(const str & in);
	};
	
	
};
#ifdef _FS_CPP
services::t_autoStartService<filesystem::fs> FS;
#else
extern services::t_autoStartService<filesystem::fs> FS;
#endif

#endif

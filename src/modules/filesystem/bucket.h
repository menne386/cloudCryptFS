// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef FILESYSTEM_BUCKET_H
#define FILESYSTEM_BUCKET_H
#include "chunk.h"
#include <vector>
#include <mutex>
#include <functional>
#include "types.h"
#include "modules/crypto/key.h"
#include "modules/util/atomic_shared_ptr.h"


namespace filesystem {
	class chunk;
	class journalEntryWrapper;
	/**
	 * @todo write docs
	 */
	typedef std::function<shared_ptr<chunk>(shared_ptr<chunk>)> loadFilter_t;
	typedef std::function<shared_ptr<chunk>(shared_ptr<chunk>)> storeFilter_t;
	template<typename T> using bucketArray = std::array<util::atomic_shared_ptr<T>,chunksInBucket>;

	class bucketChangeLog{
		private:
		std::atomic_uint index;
		std::array<std::shared_ptr<journalEntryWrapper>,1024> changeLog;
		public:
		bucketChangeLog(): index(0) {}
		
		bool addChange(std::shared_ptr<journalEntryWrapper> in);
	};

	class bucket {
		
		typedef std::recursive_mutex locktype;
		typedef std::unique_lock<locktype> lckunique;
		private:

		locktype _mut;
		util::atomic_shared_ptr<bucketArray<chunk>> chunks;
		util::atomic_shared_ptr<bucketArray<hash>> hashes;
		util::atomic_shared_ptr<bucketChangeLog> changes;
		
		const str filenamebase;
		const str myfilenamehsh() const {return filenamebase+".hsh"; }
		const str myfilenamechnk() const {return filenamebase+".chnk"; }
		
		std::shared_ptr<crypto::key> _key;
		crypto::protocolInterface * _protocol;
		shared_ptr<bucketArray<chunk>> loadChunks(void);
		shared_ptr<bucketArray<hash>> loadHashes(void);
		std::atomic_int chunkChangesSinceLoad,hashChangesSinceLoad;
		loadFilter_t loadFilter;
		storeFilter_t storeFilter;

		public:
		bucket(const str & file,std::shared_ptr<crypto::key> ikey,crypto::protocolInterface * iprotocol);
		~bucket();
		
		
		void setLoadFilter(loadFilter_t in) { loadFilter = in; };
		void setStoreFilter(storeFilter_t in) { storeFilter = in; };
		void del(void);
		void migrateTo(bucket * targetBucket);
		
		std::shared_ptr<chunk> getChunk(int64_t id);
		
		std::shared_ptr<hash> getHash(int64_t id);
		
		void putHashAndChunk(int64_t id,std::shared_ptr<hash> h,std::shared_ptr<chunk> c);

		void clearHashAndChunk(int64_t id);

		void hashChanged(void) { ++hashChangesSinceLoad; }
		
		void putHashedChunk(bucketIndex_t idx,const script::int_t irefcnt,std::shared_ptr<chunk> c);
		
		void store(bool clearCache = false);
		
		void addChange(std::shared_ptr<journalEntryWrapper> in);
	};

}

#endif // FILESYSTEM_BUCKET_H

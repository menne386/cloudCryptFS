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



namespace filesystem {
	class chunk;
	/**
	 * @todo write docs
	 */
	typedef std::function<shared_ptr<chunk>(shared_ptr<chunk>)> loadFilter_t;
	typedef std::function<shared_ptr<chunk>(shared_ptr<chunk>)> storeFilter_t;
	class bucket {
		typedef std::recursive_mutex locktype;
		typedef std::unique_lock<locktype> lckunique;
		private:

		locktype _mut;
		std::vector<shared_ptr<chunk>> chunks;
		std::vector<shared_ptr<hash>> hashes;
		str filename;
		std::shared_ptr<crypto::key> _key;
		crypto::protocolInterface * _protocol;
		void loadChunks(void);
		void loadHashes(void);
		std::atomic_int changesSinceLoad,hashesLoaded,chunksLoaded;
		loadFilter_t loadFilter;
		storeFilter_t storeFilter;

		public:
		bucket(const str & file,std::shared_ptr<crypto::key> ikey,crypto::protocolInterface * iprotocol);
		~bucket();
		
		str getFilename() {return filename;};
		
		void setLoadFilter(loadFilter_t in) { loadFilter = in; };
		void setStoreFilter(storeFilter_t in) { storeFilter = in; };
		void del(void);
		void migrateTo(bucket * targetBucket);
		void clearCache(void);
		
		std::shared_ptr<chunk> getChunk(int64_t id);
		void putChunk(int64_t id,std::shared_ptr<chunk> c);
		
		std::shared_ptr<hash> getHash(int64_t id);
		void putHash(int64_t id,std::shared_ptr<hash> c);

		void hashChanged(void) { changesSinceLoad++; }
		
		void putHashedChunk(bucketIndex_t idx,const script::int_t irefcnt,std::shared_ptr<chunk> c);
		
		void store(void);
	};

}

#endif // FILESYSTEM_BUCKET_H

// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef _FILESYSTEM_STORAGE_H
#define _FILESYSTEM_STORAGE_H

#include "modules/services/serviceHandler.h"
#include "modules/util/protected_unordered_map.h"
#include "locks.h"
#include <set>
#include "hash.h"
#include "hash.h"

namespace filesystem {

	using services::service;

	class chunk;
	class bucket;
	class hashbucket;
	class bucketaccounting;

	class bucketInfo {
	private:
		void loadHashesFromBucket(uint64_t id, std::vector<bucketIndex_t>& postList, uint64_t &numLoaded);
		bool meta = false;
		locktype _mut;
		crypto::protocolInterface* protocol;
	public:
		bucketInfo(crypto::protocolInterface* p, bool isMeta);
		
		unique_ptr<bucketaccounting> accounting;
		util::protected_unordered_map<uint64_t, shared_ptr<bucket>> loaded;
		util::protected_unordered_map<const crypto::sha256sum, std::shared_ptr<hash>> hashesIndex;

		void clear(void);
		void createNewBucket(uint64_t id);
		void removeBucket(uint64_t id);
		bucket* getBucket(uint64_t id);
		shared_ptr<chunk> getChunk(const bucketIndex_t& index);
		shared_ptr<hash> getHash(const bucketIndex_t& index);

		void loadBuckets(script::JSONPtr metaInfo,const str & name);
	};


	class storage : public service {
	private:
		str _path;
		unique_ptr<crypto::protocolInterface> protocol;

	public:
		srvSTATICDEFAULTNEWINSTANCE(storage);

		storage();
		~storage();

		crypto::protocolInterface* prot();

		unique_ptr<bucketInfo> metaBuckets, buckets;

		void setPath(const char* ipath);
		const str& getPath();

		void migrate(unique_ptr<crypto::protocolInterface> iprot);

		bool initStorage(unique_ptr<crypto::protocolInterface> iprot);

		void storeAllData();

		std::shared_ptr<hash> getHash(const bucketIndex_t& id);
		std::shared_ptr<hash> newHash(const crypto::sha256sum& in, std::shared_ptr<chunk> c);


		str getBucketFilename(int64_t id, bool metaBucket, crypto::protocolInterface* prot);


	};
}

#ifdef _FILESYSTEM_STORAGE_CPP
services::t_autoStartService<filesystem::storage> STOR;
#else
extern services::t_autoStartService<filesystem::storage> STOR;
#endif



#endif


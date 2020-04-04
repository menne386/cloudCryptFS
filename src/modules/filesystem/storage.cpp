// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
/**
 * storage+bucketInfo class is responsible for the lower storage layer of the filesystem.
 * 
 * It will maintain 2 maps of buckets: 1 for metaData and 1 for Actual data.
 * 
 * Deduplication is handled by the newHash function.
 * 
 */
#define _FILESYSTEM_STORAGE_CPP
#include "storage.h"
#include "fs.h"
#include "hash.h"
#include "chunk.h"
#include "bucket.h"
#include "bucketaccounting.h"
#include "modules/util/files.h"

#ifndef _WIN32
#include <unistd.h>
#else
#include <direct.h>
#undef ERROR
#undef max
#endif


using namespace filesystem;

bucketInfo::bucketInfo(crypto::protocolInterface* p, bool isMeta) {
	_ASSERT(p != nullptr);
	meta = isMeta;
	protocol = p;
	clear();

}
void bucketInfo::clear(void) {
	const bool msg = hashesIndex.size() > 0 || loaded.size() > 0;
	
	if (msg) {
		STOR->srvMESSAGE("Clearing ", meta ? "meta" : "", " hashes");
	}
	auto hl = hashesIndex.list();
	for (auto& i : hl) {
		if (i) {
			i->rest();
		}
	}

	hashesIndex.clear();//@todo: potential deadlock, perhaps use shared_recursive mutex?

	if (msg) {
		STOR->srvMESSAGE("Clearing ", meta ? "meta" : "", " buckets");
	}
	auto l = loaded.list();
	//for()
	for (auto& i : l) {
		for (my_size_t a = 0; a < chunksInBucket; a++) {
			auto hsh = i->getHash(a);
			if (hsh) {
				hsh->rest();
			}
		}
	}
	loaded.clear();//@todo: deadlock: this will try to rest the hashes, the hashes will

	if (msg) {
		STOR->srvMESSAGE("Cleared ", meta ? "meta" : "", " hashes & buckets");
	}

}


void bucketInfo::createNewBucket(uint64_t id) {
	FS->srvDEBUG("Adding ", meta ? "meta" : "", " bucket ", id);
	//Nothing to see/do here
}
void bucketInfo::loadHashesFromBucket(uint64_t id, std::vector<bucketIndex_t>& postList, uint64_t& numLoaded) {
	try {

		//srvMESSAGE("loading hashes from ",fn);
		auto* hb = getBucket(id);
		_ASSERT(hb != nullptr);

		//unsigned numInner  = 0;
		for (unsigned a = 0; a < chunksInBucket; a++) {
			auto hsh = hb->getHash(a);
			const bucketIndex_t b{ id,a };
			if (hsh != nullptr) {

				//++numInner;
				if (meta == false) {
					hashesIndex.insert(hsh->getHashPrimitive(), hsh);
					_ASSERT(b == hsh->getBucketIndex());
					_ASSERT(hsh->getRefCnt() > 0);
					++numLoaded;
				}
				else {
					auto chunk = hb->getChunk(a);
					_ASSERT(chunk != nullptr);
					if (chunk->as<inode_header_only>()->header.type == inode_type::NONE) {
						postList.push_back(b);
					}
					else {
						++numLoaded;
					}
					//if(meta) {_ASSERT(hb->getChunk(a)!=nullptr);}

				}
			}
			else {
				if (!(b == fs::rootIndex && meta == false)) {
					postList.push_back(b);
				}
			}
		}

		//srvMESSAGE("loading ",numInner," hashes from ",id);
		//for all hashes 
		//for()

		//the hashes table gets filled with empty, invalid hashes: these should be deleted.

	}
	catch (std::exception & e) {
		FS->srvERROR("Failed to load hashes for ", meta ? "meta" : "", " bucket:", id, ": ", e.what());
	}

}
void bucketInfo::removeBucket(uint64_t id) {
	STOR->srvMESSAGE("Removing ", meta ? "meta" : "", " bucket ", id);
	loaded.erase(id);
}

bucket* bucketInfo::getBucket(uint64_t id) {

	_ASSERT(protocol != nullptr);
	auto it = loaded.get(id);
	if (it) {
		return it.get();
	}
	auto fn = STOR->getBucketFilename(id, meta, protocol);
	loaded.insert(id, std::make_shared<bucket>(fn, meta ? protocol->getProtoEncryptionKey() : protocol->getEncryptionKey(), protocol));

	auto* ret = loaded.get(id).get();
	_ASSERT(ret != nullptr);
	return ret;
}

shared_ptr<chunk> bucketInfo::getChunk(const bucketIndex_t& index) {
	return getBucket(index.bucket())->getChunk(index.index());
}

shared_ptr<hash> bucketInfo::getHash(const bucketIndex_t& index) {
	return getBucket(index.bucket())->getHash(index.index());
}

void filesystem::bucketInfo::loadBuckets(script::JSONPtr metaInfo,const str & name) {
	auto start = std::chrono::high_resolution_clock::now();
	uint64_t numLoaded = 0;
	std::set<uint64_t> initList;
	std::vector<bucketIndex_t> postList;
	using namespace script::SLT;
	auto l = metaInfo->getSize(name);
	STOR->srvMESSAGE("loading ", l," ", name);
	auto* ptr = &metaInfo->get<I>(name, 0, l);
	for (uint64_t a = 0; a < l; ++a) {
		initList.insert(ptr[a]);
		loadHashesFromBucket(ptr[a],postList,numLoaded);
	}
	accounting = make_unique<bucketaccounting>(initList, this);
	while (postList.empty() == false) {
		accounting->post(postList.back());
		postList.pop_back();
	}
	auto finish = std::chrono::high_resolution_clock::now();
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start);
	STOR->srvMESSAGE("Loaded ", numLoaded, " items from ", loaded.size()," ", name, " in ", milliseconds.count(),"ms");

}


filesystem::storage::storage() : service("STORAGE") {
	std::array<char, 4096> wdpath;
#ifdef _WIN32
	_path = _getcwd(wdpath.data(), (int)wdpath.size());
#else
	_path = getcwd(wdpath.data(), wdpath.size());
#endif
	if (_path.back() != '/') {
		_path.push_back('/');
	}

}

filesystem::storage::~storage() {
	if (buckets)buckets->clear();
	if (metaBuckets)metaBuckets->clear();

	srvMESSAGE("remove lck: ", remove(str(_path + "._lock").c_str()));
}

crypto::protocolInterface* storage::prot() { 
	_ASSERT(protocol!=nullptr); 
	return protocol.get(); 
}

void storage::setPath(const char* ipath) {
	_path = ipath;

}

const str& storage::getPath() {
	return _path;
}

void filesystem::storage::migrate(unique_ptr<crypto::protocolInterface> iprot) {
	for (auto& bb : metaBuckets->loaded.clone()) {
		srvMESSAGE("Converting meta bucket ", bb.first);
		auto newFn = getBucketFilename(bb.first, true, iprot.get());
		auto newBucket = std::make_unique<bucket>(newFn, iprot->getProtoEncryptionKey(), iprot.get());
		bb.second->migrateTo(newBucket.get());
		newBucket->store();
		bb.second->del();
		bb.second = std::move(newBucket);
		metaBuckets->loaded.insert(bb.first, bb.second);
	}
	for (auto& bb : buckets->loaded.clone()) {
		srvMESSAGE("Converting bucket ", bb.first);
		auto newFn = getBucketFilename(bb.first, false, iprot.get());
		auto newBucket = std::make_unique<bucket>(newFn, iprot->getEncryptionKey(), iprot.get());
		bb.second->migrateTo(newBucket.get());
		newBucket->store();
		bb.second->del();
		bb.second = std::move(newBucket);
		buckets->loaded.insert(bb.first, bb.second);
	}

	protocol = std::move(iprot);

}


bool filesystem::storage::initStorage(unique_ptr<crypto::protocolInterface> iprot) {
	protocol = std::move(iprot);
	_ASSERT(protocol->getProtoEncryptionKey() != nullptr);
	unsigned waits = 0;
	while (util::fileExists(_path + "._lock")) {
		srvWARNING("filesystem is currently locked... waiting 1sec (remove ._lock if you believe this to be in error)");
		std::this_thread::sleep_for(std::chrono::seconds(1));
		++waits;
	}
	srvMESSAGE("adding ._lock");
	util::putSystemString(_path + "._lock", "test");


	buckets = make_unique<bucketInfo>(protocol.get(), false);
	metaBuckets = make_unique<bucketInfo>(protocol.get(), true);


	return true;
}

void filesystem::storage::storeAllData() {
	srvDEBUG("storing metaBucket data");
	auto mbl = metaBuckets->loaded.list();
	for (auto& i : mbl) {
		if (i) {
			i->store();//store operation will keep everything in memory but stores a copy to dsk by default
		}
	}

	srvDEBUG("storing & clearing bucket data");
	auto bl = buckets->loaded.list();
	for (auto& i : bl) {
		i->store(true);//store(true) operation will clear cache and stores a copy to dsk.
	}
}

std::shared_ptr<hash> storage::getHash(const bucketIndex_t& in) {
	auto ret = buckets->getHash(in);
	if (ret == nullptr) {
		srvERROR("Missing hash with ID: ", in);
	}
	return ret;
}



std::shared_ptr<hash> storage::newHash(const crypto::sha256sum& in, std::shared_ptr<chunk> c) {
	//should check if this hash already exists in the filesystem:
	{
		auto it = buckets->hashesIndex.get(in);
		if (it) {
			if (!it->compareChunk(c)) {
				srvERROR("HASH COLLISION in hash: ", in.toShortStr());
			}
			return it;
		}
	}
	//Make new metaData in hashes:

	{
		auto bucket = buckets->accounting->fetch();
		//CLOG("Fetched: ",bucket.fullindex," b:",bucket.bucket," i:",bucket.index);
		auto newHash = std::make_shared<hash>(in, bucket, 0, c);
		//Put both hash and chunk into storage (instead of on hash rest!)
		buckets->getBucket(bucket.bucket())->putHashAndChunk(bucket.index(), newHash, c);

		buckets->hashesIndex.insert(in, newHash);
		//return the new hash object
		return newHash;
	}
}


str storage::getBucketFilename(int64_t id, bool metaBucket, crypto::protocolInterface* prot) {
	_ASSERT(prot != nullptr);
	str hsh = metaBucket ? prot->getMetaBucketFileName(id) : prot->getBucketFileName(id);
	util::MKDIR(_path + "dta");
	util::MKDIR(_path + "dta/" + hsh.substr(0, 2));
	return _path + "dta/" + hsh.substr(0, 2) + "/" + hsh;
}



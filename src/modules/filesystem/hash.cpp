// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
/**
 * The hash class contains metaData for a chunk, contains reference count, a sha256sum and such
 * the incRefCnt will check the hash is not flagged for deletion
 * the decRefCnt will remove the hash from the storage layer.
 * 
 * the read and write operation will pull the associated chunk from the bucket.
 * 
 * All operation will inform lower storage layer of changes so the lower layer knows when to write buckets.
 */
#include "hash.h"
#include "chunk.h"
#include "bucket.h"
#include "bucketaccounting.h"
#include "storage.h"
#include <mutex>
using namespace filesystem;

str filesystem::to_string(const bucketIndex_t & in) {
	return util::to_string(in.bucket())+"#"+util::to_string(in.index());
}

//protect hash with mutex!
void hash::clearFlags(flagtype in) {
	auto expected = flags.load();
	while(!flags.compare_exchange_strong(expected,(expected|in) ^in)) {
		;//spin
	}
}


void hash::setFlags(flagtype in) {
	auto expected = flags.load();
	while(!flags.compare_exchange_strong(expected,expected|in)) {
		;//spin
	}
}

bool hash::isFlags(flagtype in) {
	return (flags.load() & in) > 0;
}


script::int_t hash::getRefCnt() {
	return refcnt.load();
}

script::int_t hash::incRefCnt() {
	++refcnt;
	if(isFlags(FLAG_NOAUTOLOAD|FLAG_NOAUTOSTORE)==false) {
		_ASSERT(isFlags(FLAG_DELETED)==false);//Never revide a dead hash!
		STOR->buckets->getBucket(bucketIndex.bucket())->hashChanged();
	}
	return refcnt;
}

script::int_t hash::decRefCnt() {
	--refcnt;
	if(isFlags(FLAG_NOAUTOLOAD|FLAG_NOAUTOSTORE|FLAG_DELETED)==false) {
		if (refcnt == 0) {
			setFlags(FLAG_DELETED);
			auto hsh = STOR->buckets->hashesIndex.get(_hsh);
			_ASSERT(hsh.get()==this);
			if (STOR->buckets->hashesIndex.erase(_hsh) == 1) {
				//srvDEBUG("Posting hash+bucket: ",in.toShortStr(),bucket.fullindex);
				STOR->buckets->getBucket(bucketIndex.bucket())->clearHashAndChunk(bucketIndex.index());
				STOR->buckets->accounting->post(bucketIndex);
			} else {
				STOR->srvERROR("Failed to delete hash ",_hsh.toShortStr(), " from global index");
			}
			clearData();
		}
		STOR->buckets->getBucket(bucketIndex.bucket())->hashChanged();
	}
	return refcnt;
}

bool hash::compareChunk(shared_ptr<chunk> c) {
	auto d = data(true);
	_ASSERT(d!=nullptr);
	return d->compareChunk(c);
}

std::shared_ptr<chunk> hash::data(bool load) {
	std::shared_ptr<chunk> ret = _data;
	if(ret==nullptr && isFlags(FLAG_NOAUTOLOAD)==false && load) {
		ret = STOR->buckets->getChunk(bucketIndex);
		_data = ret;
	}
	return ret;
}
bool hash::clearData() {
	if(isFlags(FLAG_NOAUTOSTORE) == false) {
		auto T = _data.exchange(std::shared_ptr<chunk>());
		if(T!=nullptr) {
			return true;
		}
	}
	return false;
}
bool hash::hasData() { return _data.load()!=nullptr; }


filesystem::hash::hash(const crypto::sha256sum & ihash, const bucketIndex_t ibucket, const script::int_t irefcnt, std::shared_ptr<chunk> idata, flagtype iflags): _hsh(ihash),bucketIndex(ibucket),refcnt(irefcnt), _data(idata) ,flags(iflags){
}

hash::~hash() {
	rest();
}



void hash::read(my_off_t offset,my_size_t size,unsigned char * output) {
	auto d = data(true);
	_ASSERT(d!=nullptr);
	d->read(offset,size,output);
}

hashPtr hash::write(my_off_t offset,my_size_t size,const unsigned char * input) {
	auto d = data(true);
	_ASSERT(d!=nullptr);
	auto newChunk = d->write(offset,size,input);
	//Hash the new chunk + compare to current hash: if same, return nullptr
	auto newHash = newChunk->getHash();
	if( _hsh==newHash) {
		return nullptr;
	}
	
	//Not same: create new hash in filesystem
	return STOR->newHash(newHash,newChunk);
}

bool hash::rest(void) {
	return clearData();//Currently a clearData operation is exactly the same a "rest" operation
}



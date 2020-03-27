// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#include "hash.h"
#include "chunk.h"
#include "bucket.h"
#include "bucketaccounting.h"
#include "fs.h"
#include <mutex>
using namespace filesystem;

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
		FS->buckets->getBucket(bucketIndex.bucket)->hashChanged();
	}
	return refcnt;
}

script::int_t hash::decRefCnt() {
	--refcnt;
	if(isFlags(FLAG_NOAUTOLOAD|FLAG_NOAUTOSTORE|FLAG_DELETED)==false) {
		if (refcnt == 0) {
			setFlags(FLAG_DELETED);
			auto hsh = FS->buckets->hashesIndex.get(_hsh);
			_ASSERT(hsh.get()==this);
			if (FS->buckets->hashesIndex.erase(_hsh) == 1) {
				//srvDEBUG("Posting hash+bucket: ",in.toShortStr(),bucket.fullindex);
				FS->buckets->accounting->post(bucketIndex);
			} else {
				FS->srvERROR("Failed to delete hash ",_hsh.toShortStr(), " from global index");
			}
			clearData();
		}
		FS->buckets->getBucket(bucketIndex.bucket)->hashChanged();
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
		ret = FS->buckets->getBucket(bucketIndex.bucket)->getChunk(bucketIndex.index);
		_data = ret;
	}
	return ret;
}
void hash::clearData() {
	if(isFlags(FLAG_NOAUTOSTORE) == false) {
		_data = std::shared_ptr<chunk>();
	}
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
	return FS->newHash(newHash,newChunk);
}

bool hash::rest(void) {
	auto d = data();
	if(d) {
		if (refcnt > 0 && isFlags(FLAG_DELETED|FLAG_NOAUTOSTORE) == false) {
			FS->buckets->getBucket(bucketIndex.bucket)->putChunk(bucketIndex.index, d);
		}
		clearData();
		return true;
	}
	return false;
}



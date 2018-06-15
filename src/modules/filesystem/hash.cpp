// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#include "hash.h"
#include "chunk.h"
#include "bucket.h"
#include "fs.h"
#include <mutex>
using namespace filesystem;

//protect hash with mutex!

script::int_t hash::getRefCnt() {
	return refcnt.load();
}

script::int_t hash::incRefCnt() {
	++refcnt;
	return refcnt;
}

script::int_t hash::decRefCnt() {
	--refcnt;
	return refcnt;
}


filesystem::hash::hash(const crypto::sha256sum & ihash, const bucketIndex_t ibucket, const script::int_t irefcnt, std::shared_ptr<chunk> idata): _hsh(ihash),bucketIndex(ibucket),refcnt(irefcnt), data(idata) {
}

hash::~hash() {
	rest();
}



void hash::read(my_off_t offset,my_size_t size,unsigned char * output) {
	if(data==nullptr) {
		//Load Bucket from FS: get chunk from bucket
		data = FS->buckets->getBucket(bucketIndex.bucket)->getChunk(bucketIndex.index);
	}
	_ASSERT(data!=nullptr);
	data->read(offset,size,output);
}

hashPtr hash::write(my_off_t offset,my_size_t size,const unsigned char * input) {
	if(data==nullptr) {
		//Load Bucket from FS: get chunk from bucket
		data = FS->buckets->getBucket(bucketIndex.bucket)->getChunk(bucketIndex.index);
	}
	_ASSERT(data!=nullptr);
	auto newChunk = data->write(offset,size,input);
	//Hash the new chunk + compare to current hash: if same, return nullptr
	auto newHash = newChunk->getHash();
	if( _hsh==newHash) {
		return nullptr;
	}
	
	//Not same: create new hash in filesystem
	return FS->newHash(newHash,newChunk);
}

bool hash::rest(void) {
	bool ret = false;
	if (refcnt == 0) {
		FS->removeHash(_hsh, bucketIndex);
		data = nullptr;
	}
	if(data) {
		if (refcnt > 0) {
			FS->buckets->getBucket(bucketIndex.bucket)->putChunk(bucketIndex.index, data);
		}
		data = nullptr;
		ret = true;
	}
	return ret;
}



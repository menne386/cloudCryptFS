// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#include "bucket.h"
#include "chunk.h"
#include "main.h"
#include <fstream>
#include <mutex>
#include "modules/crypto/block.h"
#include "modules/crypto/protocol.h"
#include "modules/util/files.h"
#include "modules/util/endian.h"
#include "fs.h"
using namespace filesystem;

//protect bucket with mutex

struct serializedHash{
	bucketIndex_t bucket;
	script::int_t refcnt;
	crypto::sha256sum hash;
};



const auto byteSizeChunks = (chunksInBucket * chunkSize);
const auto byteSizeHashes = (chunksInBucket * sizeof(serializedHash));


void bucket::loadHashes(void) {
	lckunique lck(_mut);
	if(hashesLoaded) {
		return;
	}

	hashes.resize(chunksInBucket);
	

	const auto byteSizeEncryptionOverhead = _protocol->getIVSize()+_protocol->getTagSize();
	auto cipher = util::getSystemString(filename,byteSizeChunks+byteSizeEncryptionOverhead);
	if(cipher.empty()==false) {
		_ASSERT(cipher.size()==byteSizeHashes+byteSizeEncryptionOverhead);
		crypto::block b(_key,_protocol,cipher,crypto::blockInput::CIPHERTEXT_IV);
		auto cleartext = b.getClearText();
		if(cleartext.empty()) {
			throw std::logic_error(str("Failed to decrypt hashes in"+filename).c_str());
		}
		if(cleartext.size()!=byteSizeHashes) {
			CLOG(cleartext.size(),"!=",byteSizeHashes);
			throw std::out_of_range(str("failed to load hashes from "+filename).c_str());
		}
		//load the chunks from the cleartext:
		auto * ptr = reinterpret_cast<const serializedHash*>(cleartext.data());
		
		unsigned N = 0;
		for(auto& h:hashes) {
			//CLOG("ptr->refcnt:",ptr->refcnt);
			if(ptr->refcnt>0) {
				++N;
				atomic_store(&h,std::make_shared<hash>(ptr->hash,ptr->bucket,ptr->refcnt,nullptr));
			}
			ptr++;
		}

		hashOfRefs =  getRefListHash();
		//CLOG(N);
	}
	//CLOG("bucket::load_end: ",filename);
	hashesLoaded = hashes.size();
}

size_t bucket::getRefListHash() {
	size_t N = 0;
	size_t N2 = 0;
	{
		lckunique lck(_mut);
		for (auto& h : hashes) {
			auto hh = atomic_load(&h);
			if (hh) {
				N2++;
				N += hh->getRefCnt();
			}
		}
	}
	return N+N2;
}
	

void bucket::loadChunks(void) {
	lckunique lck(_mut);
	if(chunksLoaded) {
		return;
	}
	chunk base;

	chunks.resize(chunksInBucket);

	auto cipher = util::getSystemString(filename);
	if(cipher.empty()==false) {
		//CLOG("bucket::loadChunks: ",filename);
		const auto byteSizeEncryptionOverhead = _protocol->getIVSize()+_protocol->getTagSize();
		const auto byteSizeEncryptedContent = byteSizeChunks+byteSizeEncryptionOverhead+byteSizeHashes+byteSizeEncryptionOverhead;
		_ASSERT(cipher.size()==byteSizeEncryptedContent);
		cipher.resize(byteSizeChunks+byteSizeEncryptionOverhead);
		
		crypto::block b(_key,_protocol,cipher,crypto::blockInput::CIPHERTEXT_IV);
		auto cleartext = b.getClearText();
		if(cleartext.empty()) {
			throw std::logic_error(str("Failed to decrypt chunks in "+filename).c_str());
		}
		if(cleartext.size()!=byteSizeChunks) {
			throw std::out_of_range(str("failed to load chunks from "+filename).c_str());
		}
		//load the chunks from the cleartext:
		auto * ptr = reinterpret_cast<const uint8_t*>(cleartext.data());
		
		if(loadFilter) {
			for(auto & cptr:chunks) {
				atomic_store(&cptr,loadFilter(base.write(0,chunkSize,ptr)));
				ptr+=chunkSize;
			} 
		} else {
			for(auto & cptr:chunks) {
				atomic_store(&cptr,base.write(0,chunkSize,ptr));
				ptr+=chunkSize;
			} 
		}
		//CLOG("bucket::load_end: ",filename);
	} else {
		//CLOG("bucket::create: ",filename);
		for(auto & cptr:chunks) {
			atomic_store(&cptr,std::make_shared<chunk>());
		}
	}

	changesSinceLoad = 0;
	chunksLoaded = chunks.size();
}
bucket::bucket(const str & file,std::shared_ptr<crypto::key> ikey,crypto::protocolInterface * iprotocol) :filename(file), _key(ikey), _protocol(iprotocol){
	changesSinceLoad = 0;
	hashesLoaded = 0;
	chunksLoaded = 0;
	hashOfRefs = 0;
}

bucket::~bucket() {
	if(chunksLoaded+hashesLoaded) {
		store();
	}
}

void filesystem::bucket::del(void) {
	changesSinceLoad = 0;
	lckunique lck(_mut);
	chunks.clear();
	hashes.clear();
	remove(filename.c_str());
	chunksLoaded = 0;
	hashesLoaded = 0;
	//CLOG("bucket::del: ", filename);
}

void filesystem::bucket::migrateTo(bucket * targetBucket) {
	lckunique lck(_mut);
	changesSinceLoad = 0;
	if(chunksLoaded==0) {loadChunks();}
	if(hashesLoaded==0) {loadHashes();}
	targetBucket->chunks = std::move(chunks);
	targetBucket->hashes = std::move(hashes);
	targetBucket->changesSinceLoad = 1000;
	targetBucket->chunksLoaded = chunksLoaded.load();
	targetBucket->hashesLoaded = hashesLoaded.load();
}

void filesystem::bucket::clearCache() {
	lckunique lck(_mut);
	chunksLoaded = 0;
	chunks.clear();
}

std::shared_ptr<chunk> bucket::getChunk(int64_t id) {
	if(!chunksLoaded) {loadChunks();}
	return atomic_load(&chunks.at(id));
}

void bucket::putChunk(int64_t id,std::shared_ptr<chunk> c) {
	if(!chunksLoaded) {loadChunks();}
	auto o = atomic_load(&chunks.at(id));
	if(o.get() == c.get()) {
		//Chunk is already at this spot in the bucket: no changes are made
		return;
	}
	++changesSinceLoad;
	atomic_store(&chunks.at(id),c);
}

std::shared_ptr<hash> bucket::getHash(int64_t id) {
	if(!hashesLoaded) {loadHashes();}
	return atomic_load(&hashes.at(id));
}

void bucket::putHash(int64_t id,std::shared_ptr<hash> c) {
	if(!hashesLoaded) {loadHashes();}
	++changesSinceLoad;	
	atomic_store(&hashes.at(id),c);
}

void bucket::putHashedChunk(bucketIndex_t idx,const script::int_t irefcnt,std::shared_ptr<chunk> c) {
	auto hsh = c->getHash();
	auto rootHash = make_shared<hash>(hsh,idx,irefcnt,nullptr);
	putHash(idx.index,rootHash);
	putChunk(idx.index,c);
}



void bucket::store(void) {
	auto newRefListHash = getRefListHash();
	if(changesSinceLoad>0 || newRefListHash != hashOfRefs.load()) {
		lckunique lck(_mut);
		if(changesSinceLoad==0 && newRefListHash == hashOfRefs.load()) {
			return;
		}
		const auto byteSizeEncryptionOverhead = _protocol->getIVSize()+_protocol->getTagSize();
		const auto byteSizeEncryptedContent = byteSizeChunks+byteSizeEncryptionOverhead+byteSizeHashes+byteSizeEncryptionOverhead;
		crypto::sha256sum emptyHsh(nullptr,0);
		//CLOG("bucket::store: ",filename);
		str cleartext,cleartextH,cipher,cipherH;
		
		cleartext.resize(byteSizeChunks);
		_ASSERT(cleartext.size()==byteSizeChunks);
		
		cleartextH.resize(byteSizeHashes);
		_ASSERT(cleartextH.size()==byteSizeHashes);
		
		_ASSERT(hashesLoaded!=0);
		
		//if(chunksLoaded ==0 ) {loadChunks();} //@todo: it SHOULD be possible to loose this shit....
		

		_ASSERT(hashes.size() == chunksInBucket);

		//
		if(chunksLoaded || util::fileExists(filename)==false) {
			if(chunksLoaded) {
				auto * ptr = reinterpret_cast<uint8_t*>(&cleartext[0]);
				for(auto & L:chunks) {
					auto cptr = atomic_load(&L);
					if(storeFilter) {
						auto i = storeFilter(cptr);
						i->read(0,chunkSize,reinterpret_cast<uint8_t*>(ptr));
					}else {
						cptr->read(0,chunkSize,reinterpret_cast<uint8_t*>(ptr));
					}
					ptr+=chunkSize;
				} 
			} else {
				FS->srvWARNING("Writing hashes without chunks to: ",filename);					
			}
			
			crypto::block b(_key,_protocol,cleartext,crypto::blockInput::CLEARTEXT_STOREIV );
			_ASSERT(cleartext.empty());
			cipher = b.getCipherText();
			_ASSERT(cipher.empty()==false);
		}
		
		//CLOG("bucket::store_hashes: ",filename);
		auto * hptr = reinterpret_cast<serializedHash*>(&cleartextH[0]);
		_ASSERT(hashes.size()==chunksInBucket);
		unsigned num = 0;
		for(auto & L:hashes) {
			auto h = atomic_load(&L);
			if(h) {
				++num;
				hptr->bucket = h->getBucketIndex();
				hptr->refcnt = h->getRefCnt();
				hptr->hash = h->getHashPrimitive();
			} else {
				hptr->bucket.fullindex = 0;
				hptr->refcnt = 0;
				hptr->hash = emptyHsh;
			}
			hptr++;
		}
		
		crypto::block b2(_key,_protocol,cleartextH,crypto::blockInput::CLEARTEXT_STOREIV );
		_ASSERT(cleartextH.empty());
		
		auto cipher2 = b2.getCipherText();
		_ASSERT(cipher2.empty()==false);
		
		size_t S = 0;
		if(cipher.empty()) {
			_ASSERT(util::fileExists(filename,&S) && S == byteSizeEncryptedContent);
			FS->srvDEBUG("Storing hashes in: ",filename);
			util::putSystemString(filename,cipher2,byteSizeChunks+byteSizeEncryptionOverhead);
			_ASSERT(cipher2.size()==byteSizeHashes+byteSizeEncryptionOverhead);
			_ASSERT(util::fileExists(filename,&S) && S == byteSizeEncryptedContent);
		} else {
			FS->srvDEBUG("Storing chunks+hashes in: ",filename);
			_ASSERT(cipher.size()==byteSizeChunks+byteSizeEncryptionOverhead);
			_ASSERT(cipher2.size()==byteSizeHashes+byteSizeEncryptionOverhead);
			util::putSystemString(filename,cipher+cipher2);
			_ASSERT(util::fileExists(filename,&S) && S == byteSizeEncryptedContent);
		}

		//CLOG("bucket::store_end: ",filename);
		changesSinceLoad = 0;
		hashOfRefs = newRefListHash;
	}
}

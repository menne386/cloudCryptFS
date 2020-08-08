// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
/**
 * The bucket class manages encrypted blocks of data and contains lists of chunks & hashes.
 * 
 * It will use protocol class to encrypt/decrypt data.
 * Bucket files are of a fixed size.
 */
#include "bucket.h"
#include "chunk.h"
#include "main.h"
#include <mutex>
#include <filesystem>

#include "modules/crypto/protocol.h"
#include "modules/util/files.h"
#include "modules/util/endian.h"
#include "storage.h"


using namespace filesystem;

//protect bucket with mutex

struct serializedHash{
	bucketIndex_t bucket;
	script::int_t refcnt;
	crypto::sha256sum hash;
};



const auto byteSizeChunks = (chunksInBucket * chunkSize);
const auto byteSizeHashes = (chunksInBucket * sizeof(serializedHash));


bool bucketChangeLog::addChange(std::shared_ptr<journalEntryWrapper> in) {
	auto idx = index.fetch_add(1);
	if(idx<changeLog.size()) {
		changeLog[idx] = in;
		return true;
	}
	return false;
}

shared_ptr<bucketArray<hash>> bucket::loadHashes(void) {
	lckunique lck(_mut);
	auto OH = hashes.load();
	if(OH) {
		return OH;
	}
	auto H = std::make_shared<bucketArray<hash>>();
	const str filenamehsh = myfilenamehsh();
	//const str filenamechnk = myfilenamechnk();
	
	auto cipher = util::getSystemString(filenamehsh);
	if(cipher.empty()==false) {
		str cleartext;
		try{
			_protocol->decrypt(_key,cipher,cleartext);
		} catch(std::exception & e) {
			throw std::logic_error(BUILDSTRING("Failed to decrypt hashes (",e.what(),") ",filenamehsh).c_str());
		}
		if(cleartext.size()!=byteSizeHashes) {
			throw std::out_of_range(str("failed to load hashes from "+filenamehsh).c_str());
		}
		//load the chunks from the cleartext:
		auto * ptr = reinterpret_cast<const serializedHash*>(cleartext.data());
		
		unsigned N = 0;
		for(auto& h: *H) {
			//CLOG("ptr->refcnt:",ptr->refcnt);
			if(ptr->refcnt>0) {
				++N;
				h = std::make_shared<hash>(ptr->hash,ptr->bucket,ptr->refcnt,nullptr);
			}
			ptr++;
		}
	}
	hashChangesSinceLoad = 0;
	hashes = H;
	return H;
}


shared_ptr<bucketArray<chunk>> bucket::loadChunks(void) {
	lckunique lck(_mut);
	auto OC = chunks.load();
	if(OC) {
		return OC;
	}
	chunk base;

	//const str filenamehsh = myfilenamehsh();
	const str filenamechnk = myfilenamechnk();
	auto C = std::make_shared<bucketArray<chunk>>();
	auto cipher = util::getSystemString(filenamechnk);
	if(cipher.empty()==false) {
		//CLOG("bucket::loadChunks: ",filename);
		str cleartext;
		try{
			_protocol->decrypt(_key,cipher,cleartext);
		} catch(std::exception & e) {
			util::putSystemString(STOR->getPath()+"decrypt_fail",util::getSystemString(filenamechnk));
			throw std::logic_error(BUILDSTRING("Failed to decrypt chunks (",e.what(),") ",filenamechnk).c_str());
		}	
		if(cleartext.size()!=byteSizeChunks) {
			throw std::out_of_range(str("failed to load chunks from "+filenamechnk).c_str());
		}
		//load the chunks from the cleartext:
		auto * ptr = reinterpret_cast<const uint8_t*>(cleartext.data());
		
		if(loadFilter) {
			for(auto & cptr: *C) {
				cptr = loadFilter(base.write(0,chunkSize,ptr));
				ptr+=chunkSize;
			} 
		} else {
			for(auto & cptr: *C) {
				cptr = base.write(0,chunkSize,ptr);
				ptr+=chunkSize;
			} 
		}
		//CLOG("bucket::load_end: ",filename);
	} else {
		//CLOG("bucket::create: ",filename);
		for(auto & cptr: *C) {
			cptr = std::make_shared<chunk>();
		}
	}

	chunkChangesSinceLoad = 0;
	chunks = C;
	return C;
}
bucket::bucket(const str & file,std::shared_ptr<crypto::key> ikey,crypto::protocolInterface * iprotocol) :filenamebase(file), _key(ikey), _protocol(iprotocol){
	chunkChangesSinceLoad = 0;
	hashChangesSinceLoad = 0;
}

bucket::~bucket() {
	if(chunks.load() || hashes.load()) {
		store();
	}
}

void filesystem::bucket::del(void) {
	hashChangesSinceLoad = 0;
	chunkChangesSinceLoad = 0;
	lckunique lck(_mut);
	chunks = std::shared_ptr<bucketArray<chunk>>();
	hashes = std::shared_ptr<bucketArray<hash>>();
	const str filenamehsh = myfilenamehsh();
	const str filenamechnk = myfilenamechnk();
	std::filesystem::remove(filenamehsh.c_str());
	std::filesystem::remove(filenamechnk.c_str());
	//CLOG("bucket::del: ", filename);
}

void filesystem::bucket::migrateTo(bucket * targetBucket) {
	lckunique lck(_mut);
	hashChangesSinceLoad = 0;
	chunkChangesSinceLoad = 0;
	if(chunks.load()==nullptr) {loadChunks();}
	if(hashes.load()==nullptr) {loadHashes();}
	targetBucket->chunks = chunks.load();
	targetBucket->hashes = hashes.load();
	chunks = std::shared_ptr<bucketArray<chunk>>();
	hashes = std::shared_ptr<bucketArray<hash>>();
	targetBucket->hashChangesSinceLoad = 1000;
	targetBucket->chunkChangesSinceLoad = 1000;
}

/*void filesystem::bucket::clearCache() {
	chunks = std::shared_ptr<bucketArray<chunk>>();
}*/

std::shared_ptr<chunk> bucket::getChunk(int64_t id) {
	auto C = chunks.load();
	if(!C) { C= loadChunks();}
	_ASSERT(C!=nullptr);
	return C->at(id);
}

std::shared_ptr<hash> bucket::getHash(int64_t id) {
	auto H = hashes.load();
	if(!H) {H = loadHashes(); }
	_ASSERT(H!=nullptr);
	return H->at(id);
}

void bucket::putHashAndChunk(int64_t id,std::shared_ptr<hash> h,std::shared_ptr<chunk> c) {
	auto H = hashes.load();
	if(!H) {H = loadHashes();}	
	auto C = chunks.load();
	if(!C) { C = loadChunks(); }
	_ASSERT(H!=nullptr && C!=nullptr);
	++chunkChangesSinceLoad;
	++hashChangesSinceLoad;
	H->at(id) = h;
	C->at(id) = c;
}

void bucket::clearHashAndChunk(int64_t id) {
	putHashAndChunk(id,nullptr,std::make_shared<chunk>());
}


void bucket::putHashedChunk(bucketIndex_t idx,const script::int_t irefcnt,std::shared_ptr<chunk> c) {
	auto hsh = c->getHash();
	auto rootHash = make_shared<hash>(hsh,idx,irefcnt,nullptr,hash::FLAG_NOAUTOSTORE|hash::FLAG_NOAUTOLOAD);
	putHashAndChunk(idx.index(),rootHash,c);
}



void bucket::store(bool clearCache) {
	if(hashChangesSinceLoad>0 || chunkChangesSinceLoad>0) {
		lckunique lck(_mut);
		if(hashChangesSinceLoad==0 && chunkChangesSinceLoad == 0) {
			return;
		}
		
		const str filenamehsh = myfilenamehsh();
		const str filenamechnk = myfilenamechnk();
		
		std::shared_ptr<bucketChangeLog> CL;
		CL = changes.exchange(std::shared_ptr<bucketChangeLog>());
		
		shared_ptr<bucketArray<chunk>> C;
		if(clearCache) {
			C = chunks.exchange(shared_ptr<bucketArray<chunk>>());
		} else {
			C = chunks.load();
		}
		auto H = hashes.load();
		hashChangesSinceLoad = 0;
		chunkChangesSinceLoad = 0;
		const bool haveChunks = C!=nullptr;
		const auto byteSizeEncryptionOverhead = _protocol->getIVSize()+_protocol->getTagSize();
		crypto::sha256sum emptyHsh(nullptr,0);
		str cleartext,cleartextH,cipher,cipherH;
		
		cleartext.resize(byteSizeChunks);
		_ASSERT(cleartext.size()==byteSizeChunks);
		
		cleartextH.resize(byteSizeHashes);
		_ASSERT(cleartextH.size()==byteSizeHashes);
		
		_ASSERT(H!=nullptr);
		
		//
		if(haveChunks || util::fileExists(filenamechnk)==false) {
			if(haveChunks) {
				auto * ptr = reinterpret_cast<uint8_t*>(&cleartext[0]);
				for(auto & L: *C) {
					shared_ptr<chunk> cptr = L;
					if (cptr) {
						if (storeFilter) {
							auto i = storeFilter(cptr);
							i->read(0, chunkSize, reinterpret_cast<uint8_t*>(ptr));
						}
						else {
							cptr->read(0, chunkSize, reinterpret_cast<uint8_t*>(ptr));
						}
					}
					ptr+=chunkSize;
				} 
			} else {
				STOR->srvWARNING("Writing hashes without chunks to: ",filenamehsh);					
			}

			try{
				_protocol->encrypt(_key,cleartext,cipher);
			} catch(std::exception & e) {
				throw std::logic_error(BUILDSTRING("Failed to encrypt chunks (",e.what(),") ",filenamechnk).c_str());
			}	
			_ASSERT(cipher.empty()==false);
			_ASSERT(cipher.size()==byteSizeChunks+byteSizeEncryptionOverhead);
		}
		
		//CLOG("bucket::store_hashes: ",filename);
		auto * hptr = reinterpret_cast<serializedHash*>(&cleartextH[0]);
		unsigned num = 0;
		for(auto & L: *H) {
			shared_ptr<hash> h = L;
			if(h) {
				++num;
				hptr->bucket = h->getBucketIndex();
				hptr->refcnt = h->getRefCnt();
				hptr->hash = h->getHashPrimitive();
			} else {
				hptr->bucket = 0;
				hptr->refcnt = 0;
				hptr->hash = emptyHsh;
			}
			hptr++;
		}
		if(num > 0) {
			try{
				_protocol->encrypt(_key,cleartextH,cipherH);
			} catch(std::exception & e) {
				throw std::logic_error(BUILDSTRING("Failed to encrypt hashes (",e.what(),") ",filenamehsh).c_str());
			}	
			
			_ASSERT(cipherH.empty()==false);
			_ASSERT(cipherH.size()==byteSizeHashes+byteSizeEncryptionOverhead);
			
			if(cipher.empty()==false) {
				STOR->srvDEBUG("Storing chunks in: ",filenamechnk);
				_ASSERT(util::putSystemString(filenamechnk,cipher)==true);
			}
			STOR->srvDEBUG("Storing hashes in: ",filenamehsh);
			_ASSERT(util::putSystemString(filenamehsh,cipherH)==true);
		} else {
			STOR->srvWARNING("No hashes to store, not writing ",filenamehsh," have chunks?",cipher.empty()==false);
		}

	}
}

void bucket::addChange(std::shared_ptr<journalEntryWrapper> in) {
	while(in!=nullptr) {
		std::shared_ptr<bucketChangeLog> H = nullptr;
		changes.compare_exchange_strong(H,std::make_shared<bucketChangeLog>());
		H = changes.load();
		_ASSERT(H!=nullptr);
		if(H->addChange(in)) {
			break;
		} else {
			STOR->srvWARNING("Too many changes for bucket: ",filenamebase);
			hashChangesSinceLoad++;
			chunkChangesSinceLoad++;
			store();//Storing here is the wrong way to handle this! Should make this list grow. if changes keep getting added to a log without actual changes, then we have an issue.
		}
	}
}


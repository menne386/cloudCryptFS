// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#define _FS_CPP
#include "fs.h"
#include "main.h"
#include "modules/util/str.h"
#include "modules/crypto/sha256.h"

#include "hash.h"
#include "inode.h"
#include "mode.h"
#include "chunk.h"
#include "bucket.h"
#include "bucketaccounting.h"
#include <chrono>
#include <mutex>
#include <random>
#ifndef _WIN32
#include <unistd.h>
#else
#include <direct.h>
#undef ERROR
#undef max
#endif
#include <thread>
#include <algorithm>
#include "modules/util/files.h"

using namespace filesystem;
using script::ComplexType;

const int metaDataVersion = 1;

/*
 * 
 
 
 */
namespace filesystem{
	class trackedChange{
		private:
			fs * _;
		public:
		trackedChange(fs * i) {
			_ = i;
			_->outstandingChanges++;
		}
		
		~trackedChange() {
			_->outstandingChanges--;
		}
	};
};


bucketInfo::bucketInfo(crypto::protocolInterface * p, bool isMeta) {
	_ASSERT(p!=nullptr);
	meta = isMeta;
	protocol = p;
	clear();
	
}
void bucketInfo::clear(void) {
	FS->srvMESSAGE("Clearing ",meta?"meta":""," hashes");
	auto hl = hashesIndex.list();
	for(auto & i: hl) {
		if(i) {
			i->rest();
		}
	}
	
	hashesIndex.clear();//@todo: potential deadlock, perhaps use shared_recursive mutex?
	
	FS->srvMESSAGE("Clearing ",meta?"meta":""," buckets");
	auto l = loaded.list();
	//for()
	for(auto & i: l) {
		for(my_size_t a=0;a<chunksInBucket;a++) {
			auto hsh = i->getHash(a);
			if(hsh) {
				hsh->rest();
			}
		}
	}
	loaded.clear();//@todo: deadlock: this will try to rest the hashes, the hashes will
	
	//l.clear();
	FS->srvMESSAGE("Cleared ",meta?"meta":""," hashes & buckets");
	
}


void bucketInfo::createNewBucket(uint64_t id) {
	FS->srvDEBUG("Adding ",meta?"meta":""," bucket ", id);
	//Nothing to see/do here
}
void bucketInfo::loadHashesFromBucket(uint64_t id) {
	lckguard l(_mut);	
	if(initList.find(id)!=initList.end()) {
		return;
	}
	initList.insert(id);
	try{
		
		//srvMESSAGE("loading hashes from ",fn);
		auto * hb = getBucket(id);
		_ASSERT(hb!=nullptr);
		
		//unsigned numInner  = 0;
		for(unsigned a=0;a<chunksInBucket;a++) {
			auto hsh = hb->getHash(a);
			if(hsh!=nullptr ) {
				
				//++numInner;
				if(meta==false) {
					const auto hshStr = hsh->getHashPrimitive();
					const auto hshIdx = hsh->getBucketIndex();
					hashesIndex.insert(hshStr,hsh);
					//CLOG("loading hash: ",hshIdx.fullindex," for ",meta?"meta":"data"," : ",reinterpret_cast<uint64_t>(hsh.get())," this:",reinterpret_cast<uint64_t>(this)," str:",hshStr.toLongStr());
					bucketIndex_t expected;
					expected.bucket = id;
					expected.index = a;
					_ASSERT(expected.fullindex == hshIdx.fullindex);
					_ASSERT(hsh->getRefCnt()>0);
					++numLoaded;
				} else {
					auto chunk = hb->getChunk(a);
					_ASSERT(chunk!=nullptr);
					if(chunk->as<inode_header_only>()->header.type==inode_type::NONE) {
						bucketIndex_t b;
						b.bucket = id;
						b.index = a;
						postList.push_back(b);						
					} else {
						++numLoaded;
					}
					//if(meta) {_ASSERT(hb->getChunk(a)!=nullptr);}
					
				}
			} else {
				bucketIndex_t b;
				b.bucket = id;
				b.index = a;
				postList.push_back(b);
			}
		}
		
		//srvMESSAGE("loading ",numInner," hashes from ",id);
		//for all hashes 
		//for()
		
		//the hashes table gets filled with empty, invalid hashes: these should be deleted.
		
	} catch(std::exception & e) {
		FS->srvERROR("Failed to load hashes for ",meta?"meta":""," bucket:",id,": ",e.what());
	}	
	
}
void bucketInfo::removeBucket(uint64_t id) {
	FS->srvMESSAGE("Removing ",meta?"meta":""," bucket ", id);
	loaded.erase(id);
}

bucket* bucketInfo::getBucket(uint64_t id) {
	
	_ASSERT(protocol!=nullptr);
	auto it = loaded.get(id);
	if(it) {
		return it.get();
	}
	auto fn = FS->getBucketFilename(id,meta,protocol);
	loaded.insert(id,std::make_shared<bucket>(fn,meta?protocol->getProtoEncryptionKey():protocol->getEncryptionKey(),protocol));

	auto * ret =  loaded.get(id).get();
	_ASSERT(ret!=nullptr);
	return ret;
}

shared_ptr<hash> bucketInfo::getHash(const bucketIndex_t& index) {
	return getBucket(index.bucket)->getHash(index.index);
}



fs::fs() :  service("FS") ,zeroChunk(chunk::newChunk(0,nullptr)), zeroSum(zeroChunk->getHash()), zeroHash(std::make_shared<hash>(zeroSum, bucketIndex_t(), 0, zeroChunk,hash::FLAG_NOAUTOSTORE|hash::FLAG_NOAUTOLOAD)){
	outstandingChanges=0;
	bucketIndex_t z;
	z.index = 1;
	z.bucket = 1000;
	_ASSERT(z.fullindex == 256001 ); //Assert the layout of the bucketIndex_t.

	z.bucket = 1;
	z.index = 255;
	_ASSERT(z.fullindex == 0b111111111 ); //Assert the layout of the bucketIndex_t.

	std::array<char,4096> wdpath;
#ifdef _WIN32
	_path = _getcwd(wdpath.data(),(int)wdpath.size());
#else
	_path = getcwd(wdpath.data(),wdpath.size());
#endif
	if( _path.back()!='/') {
		_path.push_back('/');
	}
	
	for(auto & i: _writeStats) {
		i = 0;
	}
	for(auto &i:_readStats) {
		i=0;
	}
}

fs::~fs() {
	srvMESSAGE("Waiting for outstanding changes");
	while(outstandingChanges.load()>0) {//@todo: timeout!
		std::this_thread::yield();
	}
	srvMESSAGE("Waiting on metadata write");
	lckguard _l(outstandingWrites);
	lckguard _lck2(actualWriting);
	srvMESSAGE("Closing files");
	if(root)root->rest();
	pathInodeCache.clear();
	inodeFileCache.clear();
	for (auto& h : openHandles) {
		if (h) {
			if(h->rest()) {
				srvWARNING(h->getPath()," was still open!");
			}
			h.reset();
		}
	}
	root.reset();
	if(buckets)buckets->clear();	
	if(up_and_running) {
		storeMetadata();
	}
	if(metaBuckets)metaBuckets->clear();
	
	srvMESSAGE("remove lck: ",remove(str(_path+"._lock").c_str()));
}

extern bool Fully_up_and_running;




void fs::setPath(const char * ipath) {
	_path = ipath;
	
}

const str & fs::getPath() {
	return _path;
}




str filesystem::fs::loadMetaDataFromINode(filesystem::inode* node) {
	str ret;
	unsigned maxSize = node->metasize;
	if(maxSize>0) {
		auto firstMetaNode = inoToChunk(node->metaID);
		auto * metaNode = firstMetaNode->as<inode_ctd>();
		
		std::shared_ptr<chunk> c;
		while(metaNode) {
			ret.append(&metaNode->charContent[0],sizeof(inode_ctd::charContent));
			if(metaNode->nextID.bucket==0) {
				break;
			}
			
			c = inoToChunk(metaNode->nextID);
			metaNode = c->as<inode_ctd>();
		}
		while(ret.empty()==false && ret.back()==0) {
			ret.pop_back();
		}
		ret.resize(maxSize,' ');
	}
	return ret;
}

void fs::storeMetaDataInINode(inode * rootNode,const str & input) {
	_ASSERT(input.size() < std::numeric_limits<uint32_t>::max());
	rootNode->metasize.store((uint32_t)input.size());
	auto c = createCtd(rootNode,true);
	unsigned offset = 0;
	for(const auto & i:input) {
		c->as<inode_ctd>()->charContent[offset] = i;
		++offset;
		if(offset>=sizeof(inode_ctd::charContent)) {
			offset = 0;
			c = createCtd(c->as<inode_ctd>());
		}
	}
}


bool fs::initFileSystem(unique_ptr<crypto::protocolInterface> iprot,bool mustCreate) {
	protocol = std::move(iprot);
	unsigned waits = 0;
	_ASSERT(protocol->getProtoEncryptionKey()!=nullptr);

	
	while(util::fileExists(_path+"._lock")) {
		srvWARNING("filesystem is currently locked... waiting 1sec (remove ._lock if you believe this to be in error)");
		std::this_thread::sleep_for(std::chrono::seconds(1));
		++waits;
	}
	srvMESSAGE("adding ._lock");
	util::putSystemString(_path+"._lock","test");	
	
	
	buckets = make_unique<bucketInfo>(protocol.get(),false);
	metaBuckets = make_unique<bucketInfo>(protocol.get(),true);
	
	auto metaInfo = ComplexType::newComplex();
	const bool foundFileSystem = (metaBuckets->getBucket(1)->getHash(0)!=nullptr);
	bucketIndex_t rootIndex;rootIndex.bucket=1;rootIndex.index=0;
	bucketIndex_t metaIndex;metaIndex.bucket=1;metaIndex.index=1;
	if(mustCreate) {
		//Create new filesystem
		if(foundFileSystem) {
			srvERROR("Filesystem exists!");
			return false;
		}
		srvMESSAGE("Creating filesystem: generating key");
		protocol->regenerateEncryptionKey();
		
		auto rootChunk = chunk::newChunk(0,nullptr);
		auto metaChunk = chunk::newChunk(0,nullptr);
		//Build up the root index;
		file::setFileDefaults(rootChunk); //Context???
		file::setDirMode(rootChunk,0777);
		rootChunk->as<inode>()->myID = rootIndex;
		rootChunk->as<inode>()->metaID = metaIndex;
		
		metaChunk->as<inode_ctd>()->myID = metaIndex;
		metaInfo->getI("metaBuckets")=1;
		metaInfo->getI("buckets")=1;
		protocol->storeEncryptionKeyToBlock(metaInfo->getOPtr("encryptionKey"));
		//store newly created metaData in metaChunk
		auto metaString = metaInfo->serialize(0);
		
		_ASSERT(metaString.size()<sizeof(inode_ctd::charContent));
		rootChunk->as<inode>()->metasize.store((uint32_t)metaString.size());
		std::copy(metaString.begin(),metaString.end(),&metaChunk->as<inode_ctd>()->charContent[0]);
		storeInode(rootChunk);
		storeInode(metaChunk);
		metaBuckets->getBucket(1)->store();
		buckets->getBucket(1)->store();
	} else {
		//Load existing filesystem
		if(!foundFileSystem) {
			srvERROR("Filesystem not found!");
			return false;			
		}
	}
	
	//Load existing filesystem
	metaBuckets->loadHashesFromBucket(1);
	//instance root node
	auto rootChunk = inoToChunk(rootIndex);
	metaIndex = rootChunk->as<inode>()->metaID;
	auto metaChunk = inoToChunk(metaIndex);

	str metaString = loadMetaDataFromINode(rootChunk->as<inode>());
	_ASSERT(metaString.size()>0);
	try{
		metaInfo->unserialize(metaString);
	} catch(std::exception & e) {
		srvERROR("Error unserializing metaInfo: ",e.what());
		return false;
	}
	{
		auto l = metaInfo->getSize("metaBuckets");
		srvMESSAGE("loading ",l," metaBuckets");
		auto *ptr = &metaInfo->getI("metaBuckets",0,l);
		for(uint64_t a=0;a<l;++a) {
			metaBuckets->loadHashesFromBucket(ptr[a]);
		}
		metaBuckets->accounting = make_unique<bucketaccounting>(metaBuckets->initList,metaBuckets.get());
		while(metaBuckets->postList.empty()==false) {
			metaBuckets->accounting->post(metaBuckets->postList.back());
			metaBuckets->postList.pop_back();
		}
		srvMESSAGE("Loaded ",metaBuckets->numLoaded," metaHashes from ",metaBuckets->loaded.size()," indices");
	}
	srvMESSAGE("loading key from metadata");
	//Load meta info from root node: (including key)
	_ASSERT(protocol->loadEncryptionKeyFromBlock(metaInfo->getOPtr("encryptionKey"))==true);
	{
		auto l = metaInfo->getSize("buckets");
		srvMESSAGE("loading ",l," buckets");
		script::int_t *ptr = &metaInfo->getI("buckets",0,l);
		for(uint64_t a=0;a<l;++a) {
			buckets->loadHashesFromBucket(ptr[a]);
		}
		buckets->accounting = make_unique<bucketaccounting>(buckets->initList,buckets.get());
		while(buckets->postList.empty()==false) {
			buckets->accounting->post(buckets->postList.back());
			buckets->postList.pop_back();
		}
		srvMESSAGE("Loaded ",buckets->numLoaded," hashes from ",buckets->loaded.size()," indices");
	}
	std::vector<permission> pPerm;
	root = make_shared<file>(rootChunk,"/",pPerm);
	pathInodeCache["/"] = rootIndex;
	inodeFileCache[rootIndex] = root;

	
	specialfile_error = std::make_shared<file>(chunk::newChunk(0,nullptr),"",pPerm);
	specialfile_error->setSpecialFile(specialFile::ERROR);
	_ASSERT(specialfile_error->isSpecial());
	
	Fully_up_and_running = true;
	up_and_running = true;
	_ASSERT(protocol->getEncryptionKey()!=nullptr);
	srvMESSAGE("up and running");	
	srvMESSAGE(FS->zeroHash()->getRefCnt()," references to zeroChunk.");
	return true;
}



metaPtr fs::mkobject(const char * filename, my_err_t & errorcode,const context * ctx) {
	srvDEBUG("mkobject ",filename);
	str parentname = getParentPath(filename);
	str childname = getChildPath(filename);
	//srvMESSAGE("mkobject 1 '",parentname,"'");
	
	if (childname.empty()) {
		errorcode = EE::exists;
		return nullptr;
	}
	
	auto parent = get(parentname.c_str());
	//srvMESSAGE("mkobject 1b ",filename);
	
	if(parent->valid()==false) {
		//srvERROR("mkobject ENOENT ",filename);
		
		errorcode = EE::entity_not_found;
		return nullptr;
	}
	
	auto newInode = chunk::newChunk(0,nullptr);
	file::setFileDefaults(newInode,ctx);
	newInode->as<inode>()->nlinks = 1;
	auto ino = newInode->as<inode>()->myID = metaBuckets->accounting->fetch();
	srvDEBUG("trying to add node ",childname," to parent path ",parentname);
	errorcode = parent->addNode(childname,newInode,false,ctx);
	if(errorcode){
		metaBuckets->accounting->post(ino);
		return nullptr;
	}
	storeInode(newInode);

	//srvMESSAGE("mkobject 4 ",filename);

	//lckshared lck2(_metadataMutex);
	//stats->getI("nodes")++;

	return newInode; 
}

void fs::migrate(unique_ptr<crypto::protocolInterface> newProtocol) {
	
	
	srvMESSAGE("Migrating from protocol ",protocol->getVersion(), " to ", newProtocol->getVersion());
	_ASSERT(newProtocol->getProtoEncryptionKey()!=nullptr);
	srvMESSAGE("Generating new key...");
	newProtocol->regenerateEncryptionKey();
	_ASSERT(newProtocol->getEncryptionKey()!=nullptr);
	auto key = script::ComplexType::newComplex();
	newProtocol->storeEncryptionKeyToBlock(key);
	root->setMetaProperty("encryptionKey",key);
	root->storeMetaProperties();
	root->rest();
	for(auto & bb:metaBuckets->loaded.clone()) {
		srvMESSAGE("Converting meta bucket ",bb.first);
		auto newFn = getBucketFilename(bb.first,true,newProtocol.get());
		auto newBucket = std::make_unique<bucket>(newFn,newProtocol->getProtoEncryptionKey(),newProtocol.get());
		bb.second->migrateTo(newBucket.get());
		newBucket->store();
		bb.second->del();
		bb.second = std::move(newBucket);
		bb.second->clearCache();
		metaBuckets->loaded.insert(bb.first,bb.second);
	}
	for(auto & bb:buckets->loaded.clone()) {
		srvMESSAGE("Converting bucket ",bb.first);
		auto newFn = getBucketFilename(bb.first,false,newProtocol.get());
		auto newBucket = std::make_unique<bucket>(newFn,newProtocol->getEncryptionKey(),newProtocol.get());
		bb.second->migrateTo(newBucket.get());
		newBucket->store();
		bb.second->del();
		bb.second = std::move(newBucket);
		bb.second->clearCache();
		buckets->loaded.insert(bb.first,bb.second);
	}
	
	protocol= std::move(newProtocol);
	

	
	
	
}



void fs::storeMetadata(void) {
	try{
		
		if(root) {
			srvDEBUG("Storing metaBucket & bucket list in root metaData");
			{
				while(1) {
					auto oldSize = metaBuckets->loaded.size();
					//should store the bucketList & metaBucket list in meta information (loop since changing the metadata may make it bigger)
					root->setMetaProperty("metaBuckets",metaBuckets->accounting->getBucketsInUse());
					root->setMetaProperty("buckets",buckets->accounting->getBucketsInUse());
					root->storeMetaProperties();
					if(oldSize==metaBuckets->loaded.size()) {
						break;
					} else {
						srvDEBUG("Storing metaBucket & bucket list in root metaData again");
					}
				}
			}
			root->rest();
			srvDEBUG("List storing done");
		}
		srvDEBUG("storing metaBucket data");
		auto mbl = metaBuckets->loaded.list();
		for(auto & i:mbl) {
			if(i) {
				i->store();//store operation will keep everything in memory but stores a copy to dsk.
			}
		}
		
		srvDEBUG("clearing bucket data");
		auto bl = buckets->loaded.list();
		for(auto & i:bl) {
			i->store();//store operation will keep everything in memory but stores a copy to dsk.
			i->clearCache(); //clearCache operation will release memory
		}
		srvDEBUG("end storeMetadata");
		srvMESSAGE("Metadata stored");
	} catch (std::exception &e) {
		srvERROR(e.what());
	}
}

metaPtr fs::createCtd(inode * prevNode, bool forMeta) {
	if(forMeta) {
		if(prevNode->metaID.fullindex==0) {
			srvDEBUG("Creating new (meta) inode_ctd record for ",prevNode->myID.fullindex);
			auto c = chunk::newChunk(0,nullptr);
			auto in = c->as<inode_ctd>()->myID = metaBuckets->accounting->fetch();
			prevNode->metaID = in;
			storeInode(c);
		}
		return inoToChunk(prevNode->metaID);
	}
	if(prevNode->nextID.fullindex==0) {
		srvDEBUG("Creating new inode_ctd record for ",prevNode->myID.fullindex);
		auto c = chunk::newChunk(0,nullptr);
		auto in = c->as<inode_ctd>()->myID = metaBuckets->accounting->fetch();
		prevNode->nextID = in;
		storeInode(c);
	}
	return inoToChunk(prevNode->nextID);
}

metaPtr fs::createCtd(inode_ctd * prevNode) {
	if(prevNode->nextID.fullindex==0) {
		srvDEBUG("Creating new inode_ctd record for ",prevNode->myID.fullindex);
		auto c = chunk::newChunk(0,nullptr);
		auto in = c->as<inode_ctd>()->myID = metaBuckets->accounting->fetch();
		prevNode->nextID = in;
		storeInode(c);
	}
	return inoToChunk(prevNode->nextID);	
}

void fs::storeInode(metaPtr in) {
	auto inodeType = in->as<inode_header_only>()->header.type;
	_ASSERT(inodeType == inode_type::NODE || inodeType == inode_type::CTD);
	switch(inodeType) {
		case inode_type::NODE:{
			auto idx = in->as<inode>()->myID;
			metaBuckets->getBucket(idx.bucket)->putHashedChunk(idx,1,in);
		}
		break;
		case inode_type::CTD:{
			auto idx = in->as<inode_ctd>()->myID;
			metaBuckets->getBucket(idx.bucket)->putHashedChunk(idx,1,in);			
		}
		break;
		default: 
		break;
	}
}



filePtr fs::get(const char * filename, my_err_t * errcode,const fileHandle H) {
	if(H> 0 && H <openHandles.size()) {
		auto handle = std::atomic_load(&openHandles[H]);
		if(handle!=nullptr) {
			//srvDEBUG("Using handle for ",handle->getPath());
			return handle;
		}
	}
	{
		lckguard l(_mut);
		auto itr = pathInodeCache.find(filename);
		if(itr!=pathInodeCache.end()) {
			return inodeToFile(itr->second,filename,errcode);

		}
	}

	const str myFN = str(filename);
	if(myFN.size()<=1) {
		return root;
	}
	str childname=getChildPath(myFN);
	str parentname=getParentPath(myFN);
	if(childname.size()>255) {
		if(errcode) { errcode[0] = EE::name_too_long; };
		return specialfile_error;
	}
	auto parentFile = get(parentname.c_str(),errcode);
	if(parentFile->valid()==false) {
		srvDEBUG("Parentfile ",parentname," not valid");
		if(errcode) { errcode[0] = EE::entity_not_found; };
		return specialfile_error;
	}
	if(parentFile->type()!=fileType::DIR) {
		srvDEBUG("Parentfile ",parentname," not dir");
		if(errcode) { errcode[0] = EE::entity_not_found; };
		return specialfile_error;
	}
	
	bucketIndex_t i;
	auto nodeError = parentFile->hasNode(childname,nullptr,&i);
	if(nodeError) {
		srvDEBUG("Parentfile ",parentname," dir entry not found: ",childname);
		if(errcode) { errcode[0] = nodeError; };
		return specialfile_error;
	}
	
	
	_ASSERT(i.bucket>0);
	
	{
		lckguard l(_mut);
		pathInodeCache[filename] = i;
	}
	std::vector<permission> pPerm = parentFile->getPathPermissions();
	permission p;
	p.gid = parentFile->gid();
	p.mode = parentFile->mode();
	p.uid = parentFile->uid();
	pPerm.push_back(p);
		
	return inodeToFile(i,filename,errcode,pPerm);



}


filePtr fs::inodeToFile(const bucketIndex_t i,const char * filename,my_err_t * errcode,const std::vector<permission> & pPerm) {
	{
		lckguard l(_mut);
		auto fileItr = inodeFileCache.find(i);
		if(fileItr!=inodeFileCache.end()) {
			return fileItr->second;
		}
	}
	
	metaPtr node = inoToChunk(i);
	_ASSERT(node->as<inode>()->myID.fullindex == i.fullindex);
	//if(errcode) {
	//	errcode[0] = getParentAndChild(filename,parent,child,childname,pPerm);
	//}


	filePtr NF = std::make_shared<file>(node,filename,pPerm);
	if(str(filename)=="/._stats") {NF->setSpecialFile(specialFile::STATS);}
	if(str(filename)=="/._meta") {NF->setSpecialFile(specialFile::METADATA);}
	{
		lckguard l(_mut);
		inodeFileCache[i] = NF;
		//CLOG("Inserting into cache: ",myFN,child->serialize(1));
	}
	
	return NF;
	
}

fileHandle fs::open(filePtr F) {
	//@todo: should warn when clearing inodeToFile cache that files can be open!
	for(fileHandle a=1;a<openHandles.size();a++) {
		if(std::atomic_load(&openHandles[a]).get()==nullptr) {
			std::atomic_store(&openHandles[a],F);
			return a;
		}
	}
	srvWARNING("fileHandles exhausted when requesting for: ",F->getPath());
	return 0;
}
void fs::close(filePtr F,fileHandle H) {
	if(H>0 && H < openHandles.size()) {
		auto handle = std::atomic_load(&openHandles[H]);
		if(handle.get() == F.get()) {
			filePtr E = nullptr;
			std::atomic_store(&openHandles[H],E);
		} else {
			srvWARNING("Failed to close fileHandle for: ",F->getPath());
		}
	}
}



my_err_t fs::_mkdir(const char * name,my_mode_t mode, const context * ctx) {
	trackedChange c(this);
	my_err_t e;
	auto newFile = mkobject(name,e,ctx);
	//CLOG("Gets here with err: ",e);
	
	if(!newFile) return e;
	srvDEBUG("Gets here!");
	file::setDirMode(newFile,mode);
	storeInode(newFile);
	//stats->getI("dirs")++;
	srvDEBUG("mkdir:",name);
	return EE::ok;
}


my_err_t fs::mknod(const char * filename, my_mode_t m, my_dev_t dev, const context * ctx) {
	trackedChange c(this);
	my_err_t e ;
	//srvMESSAGE("mknod ",filename);
	auto newFile = mkobject(filename,e,ctx);
	if(!newFile) {
		return e;
	}
	if ((m&mode::TYPE) == 0) {
		m |= mode::TYPE_REG;
	}
	file::setFileMode(newFile,m,dev);
	storeInode(newFile);
	auto f = get(filename);
	if(!f->valid()) {
		srvDEBUG("Newly created file is not valid????");
	} else {
		srvDEBUG(filename,": ",f->mode());
	}
	srvDEBUG("mknod:",filename," mode:",m," type:",m&mode::TYPE," ino:",newFile->as<inode>()->myID.fullindex);
	
	
	return EE::ok;
}
my_err_t fs::unlink(const char * filename, const context * ctx) {
	trackedChange c(this);
	srvDEBUG("unlink 1 ",filename);
	str srcParentName = getParentPath(filename);
	
	str srcChildName = getChildPath(filename);
	
	auto parent = get(srcParentName.c_str());
	
	auto f = get(filename);
	
	if (f->valid()==false) {
		srvERROR("unlink:", filename, " not found!");
		return EE::entity_not_found;
	}
	if(!f->validate_access(ctx,access::NONE,access::W,true)) {
		return EE::access_denied;
	}
	if(f->isSpecial()) {
		return EE::access_denied;
	}		
	srvDEBUG("unlink 2 ",filename);
	filePtr fileToDelete;
	
	if(f->type()==fileType::DIR) {
		if(f->isFullDir()) {
			return EE::not_empty;
		}
		fileToDelete = f;
	} else if(f->type()==fileType::FILE || f->type()==fileType::FIFO || f->type()==fileType::LNK) {
		f->removeLink();
		f->updateTimesWith(false,true,true,currentTime());
		if(f->getNumLinks()<=0) { 
			srvDEBUG(">>numLinks <=0: truncate:",filename);
			//Truncate the file, this should delete file contents etc etc.
			fileToDelete = f;
		}
	}	
	srvDEBUG("unlink 3 ",filename);
	
	
	auto ret = parent->removeNode(srcChildName,ctx);
	if(ret==EE::ok) {
		//stats->getI("nodes")--;
		if(f->type()==fileType::DIR) {
			//stats->getI("dirs")--;
		}
		
		lckguard _l2(_mut);
		if(fileToDelete) {
			srvDEBUG("shredding deleted file: ", fileToDelete->getPath());
			fileToDelete->truncate(0);
			fileToDelete->rest();
			auto inoToDel = fileToDelete->inoList();
			bucketIndex_t i;
			i.fullindex  = fileToDelete->ino();

			if(inodeFileCache.erase(i)!=1) {
				srvERROR("unlink:", filename,":",f->ino(), " failed to erase cached file entry");
			}
			fileToDelete.reset();
			f.reset();
			for(auto & li:inoToDel) {
				srvDEBUG("unlink:: posting ino ",li.fullindex);
				metaBuckets->getBucket(li.bucket)->putChunk(li.index,zeroChunk);
				auto removeHash = metaBuckets->getBucket(li.bucket)->getHash(li.index);
				if(removeHash) {
					metaBuckets->hashesIndex.erase(removeHash->getHashPrimitive());
					metaBuckets->getBucket(li.bucket)->putHash(li.index,nullptr);
					//_ASSERT(removeHash->decRefCnt()==0);
				}
				metaBuckets->accounting->post(li);
			}
		}
		
		
		if (pathInodeCache.erase(filename) != 1) {//This detach the file handle from the cache (calls from get will get return error handle)
			srvERROR("unlink:", filename,":",f->ino(), " failed to erase cached path");
		}
		
	
		unloadBuckets();//This will ensure metadata is saved.
		srvDEBUG("unlink 4a ",filename);
	}
	srvDEBUG("unlink 4b ",filename);
	return ret;
}

my_err_t fs::renamemove(const char * source,const char * dest, const context * ctx) {
	trackedChange c(this);
	str srcParentName = getParentPath(source);
	str dstParentName = getParentPath(dest);
	//srvDEBUG("a ",srcParentName," ",dstParentName);
	
	str srcChildName = getChildPath(source);
	str dstChildName = getChildPath(dest);
	
	auto srcparent = get(srcParentName.c_str());
	auto dstparent = get(dstParentName.c_str());
	
	auto srcfile = get(source);
	auto dstfile = get(dest);
	
	//srvDEBUG("b ",srcChildName," ",dstChildName);
	if(srcparent->valid()==false || srcfile->valid()==false || dstparent->valid()==false) {
		//srvDEBUG("entity_not_found:",srcparent->valid()?"":"srcparent" ,srcfile->valid()?"":"srcfile",dstparent->valid()?"":"dstparent");
		return EE::entity_not_found;
	}

	//rename returns EACCES or EPERM if the directory containing 'from' is marked sticky, and neither the containing directory nor 'from' are owned by the effective user ID
	if(!srcfile->validate_access(ctx,access::NONE,access::WX,true)) {
		return EE::access_denied;
	}

	//rename returns EACCES or EPERM if the file pointed at by the 'to' argument exists, 
	//  the directory containing 'to' is marked sticky, 
	//  and neither the containing directory nor 'to' are owned by the effective user ID
	if(dstfile->valid()) { // To exists:

		if(!dstfile->validate_access(ctx,access::NONE,access::WX,true)) {
			return EE::access_denied;
		}

		//"rename returns EEXIST or ENOTEMPTY if the 'to' argument is a directory and is not empty"
		if(dstfile->isFullDir()) {
			return EE::exists;
		}
	}
	
	
	//srvDEBUG("c");
	auto node = srcfile->getMetaChunk();
	//Add the file in the destination:
	auto error = dstparent->addNode(dstChildName,node,true,ctx);
	if(error==EE::exists) {
		//srvWARNING("rename is overwriting ",dest);
		unlink(dest,ctx);
		error = dstparent->addNode(dstChildName,node,true,ctx);
	}
	if(error) {
		return error;
	}
	//srvDEBUG("d");
	//Remove the old file in the source:
	error = srcparent->removeNode(srcChildName,ctx);
	if(error) {
		return error;
	}
	
	auto tv = currentTime();
	srcfile->updateTimesWith(false,true,true,tv);
	//srvDEBUG("erase cache:");
	lckguard lck(_mut);
	pathInodeCache.erase(source);
	pathInodeCache.erase(dest);
	return EE::ok;

	//rename returns EACCES or EPERM if the file pointed at by the 'to' argument exists, 
	//  the directory containing 'to' is marked sticky, 
	//  and neither the containing directory nor 'to' are owned by the effective user ID
	
}



metaPtr fs::inoToChunk(bucketIndex_t ino) {
	_ASSERT(ino.bucket>0);
	auto chunk = metaBuckets->getBucket(ino.bucket)->getChunk(ino.index);
	_ASSERT(chunk!=nullptr);
	return chunk;
}

void fs::removeInode(bucketIndex_t ino) {

}

void fs::unloadBuckets(void) {
	//srvMESSAGE("storing bucket cache for ",bucketsToUnload.size()," buckets");
	
	if(outstandingWrites.try_lock()) {
		srvDEBUG("Firing off store meta thread.");
		std::thread([this](){
			{
				{
					lckguard _lck(outstandingWrites);
					srvDEBUG("Hello from the writer thread");
					std::this_thread::sleep_for(std::chrono::seconds(5));
					srvDEBUG("End of sleep");
				}
				lckguard _lck2(actualWriting);
				
				srvDEBUG("write lock passed");
				storeMetadata();
				srvDEBUG("end of the from the metadata thread");
			}
		}).detach();
		outstandingWrites.unlock();
	} else {
		srvDEBUG("have outstandingMetaWrites");
	}
}
str fs::getBucketFilename(int64_t id,bool metaBucket,crypto::protocolInterface * prot) {
	_ASSERT(prot!=nullptr);
	str hsh = metaBucket? prot->getMetaBucketFileName(id) : prot->getBucketFileName(id);
	util::MKDIR( _path+"dta");
	util::MKDIR( _path+"dta/"+hsh.substr(0,2));
	return _path+"dta/"+hsh.substr(0,2)+"/"+hsh;
}


std::shared_ptr<hash> fs::getHash(const bucketIndex_t& in) {
	auto ret = buckets->getHash(in);
	if (ret == nullptr) {
		srvERROR("Missing hash with ID: ", in.fullindex);
	}
	return ret;
}


/*std::shared_ptr<hash> fs::loadHash(bucketIndex_t id) {
	auto * bucket = buckets.getBucket(id.bucket);
	_ASSERT(bucket!=nullptr);
	auto hsh = bucket->getHash(id.index);
	if(hsh) {
		srvDEBUG("loaded hash from b",id.bucket,"i",id.index);
		lckguard l(_mut);
		const auto hshStr = hsh->getHashPrimitive();
		buckets.hashesIndex[hshStr] = hsh;
		buckets.hashesIndexNumerical[id] = hsh;//
	} else {
		return nullptr;
	}
	return hsh;
}*/


std::shared_ptr<hash> fs::newHash(const crypto::sha256sum & in,std::shared_ptr<chunk> c) {
	//should check if this hash already exists in the filesystem:
	{
		auto it = buckets->hashesIndex.get(in);
		if (it) {
			if(!it->compareChunk(c)) {
				srvERROR("HASH COLLISION in hash: ",in.toShortStr());
			}
			return it;
		}
	}
	//Make new metaData in hashes:

	{
		auto bucket = buckets->accounting->fetch();
		//CLOG("Fetched: ",bucket.fullindex," b:",bucket.bucket," i:",bucket.index);
	
		auto B = buckets->getBucket(bucket.bucket);
		auto newHash = std::make_shared<hash>(in, bucket, 0, c);

		B->putHash(bucket.index,newHash);
		//B->putChunk(bucket.index,c);
		buckets->hashesIndex.insert(in,newHash);
		//return the new hash object
		return newHash;
	}
}

hashPtr fs::zeroHash() {
	return _zeroHash;
}





my_err_t fs::softlink(const char * linktarget,const char * linkname, const context * ctx) {
	trackedChange c(this);
	my_err_t e;
	auto newFile = mkobject(linkname,e,ctx);
	if(!newFile) return e;
	newFile->as<inode>()->mode |= mode::TYPE_LNK;
	e = EE::ok;
	auto fil = get(linkname,&e);
	if(e) {
		return e;
	}

	auto ret = fil->write(reinterpret_cast<const uint8_t *>(linktarget),str(linktarget).size(),0);
	fil->truncate(str(linktarget).size());
	if(ret!=(int)str(linktarget).size()) {
		return EE::entity_not_found;
	}
	fil->rest();
	return EE::ok;
}
my_err_t fs::hardlink(const char * linktarget,const char * linkname, const context * ctx) {
	trackedChange c(this);
	lckguard l(_mut);
	/*for(auto & i: _fileCache) {
		CLOG("fileCacheEntry(before): ",i.first," ",i.second->getNumLinks());
	}*/

	auto target = get(linktarget);
	if(target->valid()==false) {
		srvDEBUG("link target not found:",linktarget);
		return EE::entity_not_found;
	}
	

	
	auto parentName = getParentPath(linkname);
	auto childName = getChildPath(linkname);
	auto parent = get(parentName.c_str());
	
	
	if(parent->valid()==false) {
		srvDEBUG("link parent not found:",parentName);
		return EE::entity_not_found;
	}
	
	auto e = parent->addNode(childName,target->getMetaChunk(),false,ctx);
	if(e) {
		return e;
	}

	target->addLink();
	target->updateTimesWith(false,true,true,currentTime());
	
	srvDEBUG("hardlink:",linkname,">>",linktarget);
	return EE::ok;	
}



my_size_t fs::evictCache(const char * ipath) {
	lckguard l(_mut);
	for(auto & i: inodeFileCache) {
		i.second->rest();
	}
	
	pathInodeCache.clear();
	inodeFileCache.clear();
	
	return inodeFileCache.size();
}


str fs::getStats() {
	lckguard l(_mut);
	const auto KB = 1024;
	const auto bucketSizeInKB = (chunkSize*chunksInBucket)/KB;
	const auto chunkSizeInKB = (chunkSize)/KB;
	str C;	
	uint64_t numBuckets = buckets->accounting->getBucketsInUse().size();
	uint64_t numMetaBuckets = metaBuckets->accounting->getBucketsInUse().size();
	uint64_t hashesInMem = 0;
	std::map<str,int64_t> hashDistribution;
	uint64_t numDedupKbs = 0;
	for(auto & hsh: buckets->hashesIndex.list()) {
		if(hsh->hasData()) {
			hashesInMem ++;
		}
		auto ref = hsh->getRefCnt();
		if(ref==0 || ref == 1) {
			hashDistribution[std::to_string(ref).c_str()]++;
		} else if(ref < 5) {
			hashDistribution["1-5"]++;
		} else if(ref < 50) {
			hashDistribution["5-50"]++;
		} else {
			hashDistribution["50+"]++;
		}
		if(ref > 1) {
			numDedupKbs += (ref-1) * chunkSizeInKB;
		}
	}
	unsigned numINodes = 0;
	unsigned numINodeCTD = 0;
	unsigned freeINodes = 0;
	unsigned dirs=0,files=0,links=0,fifo=0,unknown = 0;
	
	for(auto & b: metaBuckets->loaded.list()) {
		for(unsigned a=0;a<chunksInBucket;a++) {
			auto C = b->getChunk(a);
			auto type = C->as<inode_header_only>()->header.type;
			switch(type) {
				case inode_type::NODE:
					++numINodes;
					{
						switch(C->as<inode>()->mode & mode::TYPE) {
							case mode::TYPE_DIR:++dirs;break;
							case mode::TYPE_REG:++files;break;
							case mode::TYPE_LNK:++links;break;
							case mode::TYPE_FIFO:++fifo;break;
							default: ++unknown; break;
						}
					}
					break;
				case inode_type::CTD:
					++numINodeCTD;
					break;
				default:
					freeINodes++;
					break;
			}
		}
	}

	C+= BUILDSTRING("(Dsk) Buckets: ",numBuckets," * ",bucketSizeInKB,"KB == ",(numBuckets * bucketSizeInKB)/KB,"MB\n");
	C+= BUILDSTRING("(Dsk) Hashes: ",buckets->hashesIndex.size()," * ",chunkSizeInKB,"KB == ",(buckets->hashesIndex.size()*chunkSizeInKB)/KB,"MB\n");
	C+= BUILDSTRING("(Mem) Hashes: ",hashesInMem," * ",chunkSizeInKB,"KB == ",(hashesInMem*chunkSizeInKB)/KB,"MB\n");
	C+= BUILDSTRING("(Dsk&Mem) Metabuckets: ",numMetaBuckets," * ",bucketSizeInKB,"KB == ",(numMetaBuckets * bucketSizeInKB)/KB,"MB\n");
	C+= BUILDSTRING("De-duplication stats:\n");
	for(const auto &i:hashDistribution) {
		C+= BUILDSTRING("Hashes with refcnt(",i.first,"): ",i.second,"\n");
	}
	C+= BUILDSTRING("About ~",numDedupKbs/KB,"MB de-duplicated.\n");
	
	C+= BUILDSTRING("Writes:\n");
	C+= BUILDSTRING("S  < chunk   :",_writeStats.at(0).load(),"\n");
	C+= BUILDSTRING("S == chunk   :",_writeStats.at(1).load(),"\n");
	C+= BUILDSTRING("S  < 9*chunks:",_writeStats.at(2).load(),"\n");
	C+= BUILDSTRING("S  < bucket  :",_writeStats.at(3).load(),"\n");
	C+= BUILDSTRING("S >= bucket  :",_writeStats.at(4).load(),"\n");
	
	C+= BUILDSTRING("Reads:\n");
	C+= BUILDSTRING("S  < chunk   :",_readStats.at(0).load(),"\n");
	C+= BUILDSTRING("S == chunk   :",_readStats.at(1).load(),"\n");
	C+= BUILDSTRING("S  < 9*chunks:",_readStats.at(2).load(),"\n");
	C+= BUILDSTRING("S  < bucket  :",_readStats.at(3).load(),"\n");
	C+= BUILDSTRING("S >= bucket  :",_readStats.at(4).load(),"\n");
	
	
	C+= BUILDSTRING("inodes: ",numINodes," ctd's:",numINodeCTD,"\n");
	C+= BUILDSTRING("files: ",files,"\n");
	C+= BUILDSTRING("dirs: ",dirs,"\n");
	C+= BUILDSTRING("links: ",links,"\n");
	C+= BUILDSTRING("fifo's: ",fifo,"\n");
	C+= BUILDSTRING("unknown: ",unknown,"\n");
	
	return C;
}
my_size_t fs::metadataSize() {
	lckguard l(_mut);
	return metaBuckets->loaded.size() * chunksInBucket * chunkSize;
}

my_off_t fs::readMetadata(unsigned char* buf, my_size_t size, my_off_t offset) {
	lckguard l(_mut);
	//my_size_t firstBucket = offset/(chunksInBucket * chunkSize);
	my_size_t bucketIdx = 0;

	my_off_t offsetInBuf = 0;
	my_off_t actualOffset = 0;
	for(auto & it: metaBuckets->loaded.list()) {
		for(unsigned a=0;a<chunksInBucket;a++) {
			if(offset>= actualOffset && offset<actualOffset+chunkSize) {
				auto newSize = std::min(std::min(size,(my_size_t)((actualOffset+chunkSize) - offset)),(my_size_t)chunkSize);
				it->getChunk(a)->read(std::max(offset - actualOffset,(my_off_t)0),newSize,&buf[offsetInBuf]);
				offsetInBuf += newSize;
				size -= newSize;
			}
			actualOffset+=chunkSize;
			if(size<=0) return offsetInBuf;
		}
		bucketIdx++;
	}
	return offsetInBuf;
}


std::pair<uint64_t,uint64_t> fs::getStatFS() {
	//@todo: locking?
	
	return std::make_pair<uint64_t,uint64_t>((uint64_t)chunkSize,buckets->hashesIndex.size()+metaBuckets->hashesIndex.size());
}

str fs::getParentPath(const str & in) {
	str parentname(in);
	auto pos = parentname.find_last_of('/');
	_ASSERT(pos!=str::npos);
	parentname.erase(pos,str::npos);
	if(parentname.empty()) {
		parentname = "/";
	}
	return parentname;
}

str fs::getChildPath(const str & in) {
	auto pos = in.find_last_of('/');
	_ASSERT(pos!=str::npos);
	
	return in.substr(pos+1);
	//parentname.erase(pos,str::npos);	
	//return parentname;
}


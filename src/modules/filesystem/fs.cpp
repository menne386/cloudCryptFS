// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
/**
 * fs class is responsible for maintaining files/directories.
 * 
 * receives filesystem call from fuse_main.cpp
 */
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
#include "storage.h"
#include "journal.h"
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
using namespace script::SLT;

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





fs::fs() :  service("FS") ,zeroChunk(chunk::newChunk(0,nullptr)), zeroSum(zeroChunk->getHash()), _zeroHash(std::make_shared<hash>(zeroSum, bucketIndex_t{1,0}, 1, zeroChunk,hash::FLAG_NOAUTOSTORE|hash::FLAG_NOAUTOLOAD)){
	outstandingChanges=0;
	bucketIndex_t z{1000,1};
	_ASSERT(z.fullindex() == 256001 ); //Assert the layout of the bucketIndex_t.

	z = bucketIndex_t{1,255};
	_ASSERT(z.fullindex() == 0b111111111 ); //Assert the layout of the bucketIndex_t.

	
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
	for (auto& hh : openHandles) {
		filePtr h = hh;
		if (h) {
			if(h->rest()) {
				srvWARNING(h->getPath()," was still open!");
			}
			hh=filePtr();
		}
	}
	root.reset();
	if(up_and_running) {
		storeMetadata();
	}
}

extern bool Fully_up_and_running;








str filesystem::fs::loadMetaDataFromINode(filesystem::inode* node) {
	str ret;
	unsigned maxSize = node->metasize;
	if(maxSize>0) {
		auto firstMetaNode = inoToChunk(node->metaID);
		auto * metaNode = firstMetaNode->as<inode_ctd>();
		
		std::shared_ptr<chunk> c;
		while(metaNode) {
			ret.append(&metaNode->charContent[0],sizeof(inode_ctd::charContent));
			if(!metaNode->nextID) {
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
	//@todo: should also write the node to it's bucket!
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
	STOR->initStorage(std::move(iprot));
	
	
	

	
	
	
	STOR->buckets->hashesIndex.insert(zeroSum,_zeroHash);
	
	auto metaInfo = script::make_json();
	const bool foundFileSystem = (STOR->metaBuckets->getBucket(1)->getHash(0)!=nullptr);
	if(mustCreate) {
		//Create new filesystem
		if(foundFileSystem) {
			srvERROR("Filesystem exists!");
			return false;
		}
		srvMESSAGE("Creating filesystem: generating key");
		STOR->prot()->regenerateEncryptionKey();
		
		auto rootChunk = chunk::newChunk(0,nullptr);
		auto metaChunk = chunk::newChunk(0,nullptr);
		//Build up the root index;
		file::setFileDefaults(rootChunk,0777|mode::TYPE_DIR); //Context???
		rootChunk->as<inode>()->myID = rootIndex;
		rootChunk->as<inode>()->metaID = metaIndex;
		
		metaChunk->as<inode_ctd>()->myID = metaIndex;
		(*metaInfo)["metaBuckets"]=1;
		(*metaInfo)["buckets"]=1;
		STOR->prot()->storeEncryptionKeyToBlock((*metaInfo)["encryptionKey"]);
		//store newly created metaData in metaChunk
		auto metaString = metaInfo->serialize(0);
		
		_ASSERT(metaString.size()<sizeof(inode_ctd::charContent));
		rootChunk->as<inode>()->metasize.store((uint32_t)metaString.size());
		std::copy(metaString.begin(),metaString.end(),&metaChunk->as<inode_ctd>()->charContent[0]);
		storeInode(rootChunk);
		storeInode(metaChunk);
		STOR->metaBuckets->getBucket(1)->store();
		STOR->buckets->getBucket(1)->store();
	} else {
		//Load existing filesystem
		if(!foundFileSystem) {
			srvERROR("Filesystem not found!");
			return false;			
		}
	}
	
	//Load existing filesystem:

	//instance root node
	auto rootChunk = inoToChunk(rootIndex);

	//Load the metaData string from the root Chunk:
	try{
		str metaString = loadMetaDataFromINode(rootChunk->as<inode>());
		_ASSERT(metaString.size() > 0);
		metaInfo->unserialize(metaString);
	} catch(std::exception & e) {
		srvERROR("Error unserializing metaInfo: ",e.what());
		return false;
	}
	
	srvMESSAGE("loading key from metadata");
	//Load meta info from root node: (including key)
	_ASSERT(STOR->prot()->loadEncryptionKeyFromBlock((*metaInfo)["encryptionKey"])==true);
	_ASSERT(STOR->prot()->getEncryptionKey() != nullptr);

	//Make sure the zeroHash has a place in a bucket.
	STOR->buckets->getBucket(rootIndex.bucket())->putHashAndChunk(rootIndex.index(),_zeroHash,zeroChunk);
	
	
	
	//Load metaBuckets & buckets:
	STOR->metaBuckets->loadBuckets(metaInfo, "metaBuckets");
	STOR->buckets->loadBuckets(metaInfo, "buckets");

	std::vector<permission> pPerm;
	root = make_shared<file>(rootChunk,"/",pPerm);
	pathInodeCache.insert("/",rootIndex);
	inodeFileCache.insert(rootIndex,root);

	
	specialfile_error = std::make_shared<file>(chunk::newChunk(0,nullptr),"",pPerm);
	specialfile_error->setSpecialFile(specialFile::ERROR);
	_ASSERT(specialfile_error->isSpecial());

	JOURNAL->tryReplay();
	
	Fully_up_and_running = true;
	up_and_running = true;
	srvMESSAGE("up and running");	
	srvMESSAGE(FS->zeroHash()->getRefCnt()," references to zeroChunk.");
	return true;
}



metaPtr fs::mkobject(const char * filename, my_err_t & errorcode,const context * ctx,my_mode_t type, my_mode_t mod) {
	//@todo: should fix this function to be more journaling friendly (have type parameter for each type of object added).
	_ASSERT((type&mode::TYPE)!=0);
	
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
	
	
	auto ino = STOR->metaBuckets->accounting->fetch();
	
	auto entry = JOURNAL->add(journalEntryType::mkobject,parent->bucketIdx(),ino,bucketIndex_t(),mod|type,0,childname);

	errorcode = replayEntry(entry->entry(),childname,"",ctx,entry);
	
	if(errorcode){
		STOR->metaBuckets->accounting->post(ino);
		return nullptr;
	}

	//srvMESSAGE("mkobject 4 ",filename);

	//lckshared lck2(_metadataMutex);
	//stats->getI("nodes")++;

	return STOR->metaBuckets->getChunk(ino); 
}

void fs::migrate(unique_ptr<crypto::protocolInterface> newProtocol) {
	
	
	srvMESSAGE("Migrating from protocol ", STOR->prot()->getVersion(), " to ", newProtocol->getVersion());
	_ASSERT(newProtocol->getProtoEncryptionKey()!=nullptr);
	srvMESSAGE("Generating new key...");
	newProtocol->regenerateEncryptionKey();
	_ASSERT(newProtocol->getEncryptionKey()!=nullptr);
	auto key = script::make_json();
	newProtocol->storeEncryptionKeyToBlock(key);
	root->setMetaProperty("encryptionKey",key);
	root->storeMetaProperties();
	root->rest();

	STOR->migrate(std::move(newProtocol));

}

my_err_t fs::replayEntry(const journalEntry * entry, const str & name, const str & data,const context * ctx,journalEntryPtr je) {
	//Replay the things that i can do from FS, delegate to FILE if required! :)
	switch(entry->type) {
		case journalEntryType::mkobject: {
			if(je==nullptr) {
				srvDEBUG("executing journal entry ",entry->id,": ",name, " newIno:",entry->newNode);
			}
			auto parent = inodeToFile(entry->parentNode,"",nullptr);
			_ASSERT(parent->valid());
			auto newInode = chunk::newChunk(0,nullptr);
			file::setFileDefaults(newInode,entry->mod,ctx);
			newInode->as<inode>()->myID = entry->newNode;
			_ASSERT(entry->newNode);
			
			srvDEBUG("trying to add node ",name," to parent path ",parent->getPath());
			
			auto status = parent->addNode(name,newInode,false,ctx,je);
			if(status == EE::ok) {
				storeInode(newInode);
			}
			return status;
		}break;
		
		case journalEntryType::unlink: {
			return unlinkinner(name.c_str(),ctx,je);
			//@todo:
		}break;
		
		case journalEntryType::renamemove: {
			return renamemoveinner(name.c_str(),data.c_str(),ctx,je);
		}break;
		
		default:{
			auto F = inodeToFile(entry->newNode,"",nullptr);
			return F->replayEntry(entry,name,data,ctx,je);
		}break;
	}
	
	return EE::invalid_syscall;
}



void fs::storeMetadata(void) {
	try{
		
		if(root) {
			srvDEBUG("Storing metaBucket & bucket list in root metaData");
			{
				while(1) {
					auto oldSize = STOR->metaBuckets->loaded.size();
					//should store the bucketList & metaBucket list in meta information (loop since changing the metadata may make it bigger)
					root->setMetaProperty("metaBuckets", STOR->metaBuckets->accounting->getBucketsInUse());
					root->setMetaProperty("buckets", STOR->buckets->accounting->getBucketsInUse());
					root->storeMetaProperties();
					if(oldSize== STOR->metaBuckets->loaded.size()) {
						break;
					} else {
						srvDEBUG("Storing metaBucket & bucket list in root metaData again");
					}
				}
			}
			srvDEBUG("List storing done");
		}
		STOR->storeAllData();

		srvDEBUG("end storeMetadata");
		srvMESSAGE("Metadata stored");
	} catch (std::exception &e) {
		srvERROR(e.what());
	}
}

metaPtr fs::createCtd(inode * prevNode, bool forMeta) {
	if(forMeta) {
		if(!prevNode->metaID) {
			srvDEBUG("Creating new (meta) inode_ctd record for ",prevNode->myID);
			auto c = chunk::newChunk(0,nullptr);
			auto in = c->as<inode_ctd>()->myID = STOR->metaBuckets->accounting->fetch();
			prevNode->metaID = in;
			storeInode(c);
		}
		return inoToChunk(prevNode->metaID);
	}
	if(!prevNode->nextID) {
		srvDEBUG("Creating new inode_ctd record for ",prevNode->myID);
		auto c = chunk::newChunk(0,nullptr);
		auto in = c->as<inode_ctd>()->myID = STOR->metaBuckets->accounting->fetch();
		prevNode->nextID = in;
		storeInode(c);
	}
	return inoToChunk(prevNode->nextID);
}

metaPtr fs::createCtd(inode_ctd * prevNode) {
	if(!prevNode->nextID) {
		srvDEBUG("Creating new inode_ctd record for ",prevNode->myID);
		auto c = chunk::newChunk(0,nullptr);
		auto in = c->as<inode_ctd>()->myID = STOR->metaBuckets->accounting->fetch();
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
			STOR->metaBuckets->getBucket(idx.bucket())->putHashedChunk(idx,1,in);
		}
		break;
		case inode_type::CTD:{
			auto idx = in->as<inode_ctd>()->myID;
			STOR->metaBuckets->getBucket(idx.bucket())->putHashedChunk(idx,1,in);
		}
		break;
		default: 
		break;
	}
}



filePtr fs::get(const char * filename, my_err_t * errcode,const fileHandle H) {
	if(H> 0 && H <openHandles.size()) {
		filePtr handle = openHandles[H];
		if(handle!=nullptr) {
			//srvDEBUG("Using handle for ",handle->getPath());
			return handle;
		}
	}
	{
		auto itr = pathInodeCache.get(filename);
		if(itr) {
			auto F = inodeToFile(itr,filename,errcode);
			if(F->valid()) {
				return F;
			} else {
				if(pathInodeCache.erase(filename)!=1) {
					srvERROR("Failed to clear ",filename," to inode cache");
				}
				pathInodeCache.insert("/",rootIndex);
			}
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
	
	
	_ASSERT(i);
	
	pathInodeCache.insert(filename,i);
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
		auto fPtr = inodeFileCache.get(i);
		if(fPtr) {
			return fPtr;
		}
	}
	
	metaPtr node = inoToChunk(i);
	if(node->as<inode_header_only>()->header.type!=inode_type::NODE) {
		srvWARNING("Node ",i," when loaded is not a node!");
		if(errcode) {
			*errcode = EE::entity_not_found;
		}
		return specialfile_error;
	}
	const auto & myID = node->as<inode>()->myID;
	if(myID != i) {
		srvWARNING("Node ",i," when loaded reports as ",myID);
		if(errcode) {
			*errcode = EE::entity_not_found;
		}
		return specialfile_error;
	}
	//if(errcode) {
	//	errcode[0] = getParentAndChild(filename,parent,child,childname,pPerm);
	//}


	filePtr NF = std::make_shared<file>(node,filename,pPerm);
	if(str(filename)=="/._stats") {NF->setSpecialFile(specialFile::STATS);}
	if(str(filename)=="/._meta") {NF->setSpecialFile(specialFile::METADATA);}
	
	inodeFileCache.insert(i,NF);
		//CLOG("Inserting into cache: ",myFN,child->serialize(1));
	
	return NF;
	
}

fileHandle fs::open(filePtr F) {
	//should warn when clearing inodeToFile cache that files can be open!
	for(fileHandle a=1;a<openHandles.size();a++) {
		filePtr expected = nullptr;
		if(openHandles[a].compare_exchange_strong(expected,F)) {
			return a;
		}
	}
	srvWARNING("fileHandles exhausted when requesting for: ",F->getPath());
	return 0;
}
void fs::close(filePtr F,fileHandle H) {
	if(H>0 && H < openHandles.size()) {
		if(openHandles[H].compare_exchange_strong(F,filePtr())) {
			return;
		} else {
			srvWARNING("Failed to close fileHandle for: ",F->getPath());
		}
	}
}



my_err_t fs::_mkdir(const char * name,my_mode_t mode, const context * ctx) {
	trackedChange c(this);
	my_err_t e;
	auto newFile = mkobject(name,e,ctx,mode::TYPE_DIR,mode);
	//CLOG("Gets here with err: ",e);
	
	if(!newFile) return e;

	srvDEBUG("mkdir:",name);
	return EE::ok;
}


my_err_t fs::mknod(const char * filename, my_mode_t m, my_dev_t dev, const context * ctx) {
	trackedChange c(this);
	my_err_t e ;
	//srvMESSAGE("mknod ",filename);
	if ((m&mode::TYPE) == 0) {
		m |= mode::TYPE_REG;
	}
	auto newFile = mkobject(filename,e,ctx,m&mode::TYPE,m);
	if(!newFile) {
		return e;
	}
	auto f = get(filename);
	if(!f->valid()) {
		srvDEBUG("Newly created file is not valid????");
	} else {
		srvDEBUG(filename,": ",f->mode());
	}
	srvDEBUG("mknod:",filename," mode:",m," type:",m&mode::TYPE," ino:",newFile->as<inode>()->myID);
	
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
	if(f.get()==root.get()) {
		srvERROR("Attempt to unlink the root!");
		return EE::access_denied;
	}
	srvDEBUG("unlink 2 ",filename);
	auto entry = JOURNAL->add(journalEntryType::unlink,parent->bucketIdx(),f->bucketIdx(),bucketIndex_t(),(my_mode_t)0,f->getNumLinks(),filename);
	parent.reset();
	f.reset();
	return replayEntry(entry->entry(),filename,"",ctx,entry);
}

my_err_t fs::unlinkinner(const char * filename, const context * ctx,shared_ptr<journalEntryWrapper> je) {
	str srcParentName = getParentPath(filename);
	
	str srcChildName = getChildPath(filename);
	
	auto parent = get(srcParentName.c_str());
	
	auto f = get(filename);
	
	if(je) {
		_ASSERT(je->entry()->newNode == f->bucketIdx());
		_ASSERT(je->entry()->parentNode == parent->bucketIdx());
		_ASSERT(je->entry()->offset == f->getNumLinks());
	}
		
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
	
	
	auto ret = parent->removeNode(srcChildName,ctx,je);
	if(ret==EE::ok) {
		//stats->getI("nodes")--;
		if(f->type()==fileType::DIR) {
			//stats->getI("dirs")--;
		}
		
		lckguard _l2(_mut);
		if(fileToDelete) {
			srvDEBUG("shredding deleted file: ", fileToDelete->getPath());
			auto inoToDel = fileToDelete->setDeletedAndReturnAllUsedInodes();//mark file as deleted and return all metaData blocks that were used.
			bucketIndex_t i = bucketIndex_t(fileToDelete->ino());

			if(inodeFileCache.erase(i)!=1) {
				srvERROR("unlink:", filename,":",f->ino(), " failed to erase cached file entry");
			}
			
			fileToDelete.reset();
			f.reset();
			for(auto & li:inoToDel) {
				srvDEBUG("unlink:: posting ino ",li);
				STOR->metaBuckets->getBucket(li.bucket())->clearHashAndChunk(li.index());
				STOR->metaBuckets->accounting->post(li);
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
			return EE::not_empty;
		}
	}

	auto entry = JOURNAL->add(journalEntryType::renamemove,srcparent->bucketIdx(),srcfile->bucketIdx(),dstparent->bucketIdx(),(my_mode_t)0,0,source,dest);
	
	return replayEntry(entry->entry(),source,dest,ctx,entry);
}

my_err_t fs::renamemoveinner(const char * source, const char * dest, const context * ctx,shared_ptr<journalEntryWrapper> je) {		
	str srcParentName = getParentPath(source);
	str dstParentName = getParentPath(dest);
	//srvDEBUG("a ",srcParentName," ",dstParentName);
	
	str srcChildName = getChildPath(source);
	str dstChildName = getChildPath(dest);
	
	auto srcparent = get(srcParentName.c_str());
	auto dstparent = get(dstParentName.c_str());
	
	auto srcfile = get(source);
	auto dstfile = get(dest);	
	
	if(je) {
		_ASSERT(je->entry()->parentNode == srcparent->bucketIdx());
		_ASSERT(je->entry()->newNode == srcfile->bucketIdx());
		_ASSERT(je->entry()->newParentNode == dstparent->bucketIdx());
	}
	//srvDEBUG("c");
	auto node = srcfile->getMetaChunk();
	//Add the file in the destination:
	auto error = dstparent->addNode(dstChildName,node,true,ctx,je);
	if(error==EE::exists) {
		//srvWARNING("rename is overwriting ",dest);
		unlink(dest,ctx);
		error = dstparent->addNode(dstChildName,node,true,ctx,je);
	}
	if(error) {
		return error;
	}
	//srvDEBUG("d");
	//Remove the old file in the source:
	error = srcparent->removeNode(srcChildName,ctx,je);
	if(error) {
		return error;
	}
	
	auto tv = currentTime();
	srcfile->updateTimesWith(false,true,true,tv);
	//srvDEBUG("erase cache:");
	pathInodeCache.erase(source);
	pathInodeCache.erase(dest);
	return EE::ok;

	//rename returns EACCES or EPERM if the file pointed at by the 'to' argument exists, 
	//  the directory containing 'to' is marked sticky, 
	//  and neither the containing directory nor 'to' are owned by the effective user ID
	
}



metaPtr fs::inoToChunk(bucketIndex_t ino) {
	_ASSERT(ino);
	auto chunk = STOR->metaBuckets->getBucket(ino.bucket())->getChunk(ino.index());
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



hashPtr fs::zeroHash() {
	return _zeroHash;
}





my_err_t fs::softlink(const char * linktarget,const char * linkname, const context * ctx) {
	trackedChange c(this);
	my_err_t e;
	auto newFile = mkobject(linkname,e,ctx,mode::TYPE_LNK,0777);
	if(!newFile) return e;
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
	
	auto e = parent->addNode(childName,target->getMetaChunk(),false,ctx,nullptr);
	if(e) {
		return e;
	}

	target->addLink();
	target->updateTimesWith(false,true,true,currentTime());
	
	srvDEBUG("hardlink:",linkname,">>",linktarget);
	return EE::ok;	
}



my_size_t fs::evictCache(const char * ipath) {
	//auto it = inodeFileCache.find(ipath);
	//@todo: should erase from cache all nodes under path
	//@todo: current issue is that nodes may be dropped from cache that are still in an operation
	/*auto inode = pathInodeCache.get(ipath);
	if(inode) {
		pathInodeCache.erase(ipath);
		
	}*/
	
	/* @todo: this implementation is wrong.
	 * 
	 * auto cacheList = inodeFileCache.list();
	for(auto i: cacheList) {
		i->rest();
	}
	
	pathInodeCache.clear();
	inodeFileCache.clear();
	
	pathInodeCache.insert("/",rootIndex);
	inodeFileCache.insert(rootIndex,root);*/
	
	return inodeFileCache.size();
}


str fs::getStats() {
	lckguard l(_mut);
	const auto KB = 1024;
	const auto bucketSizeInKB = (chunkSize*chunksInBucket)/KB;
	const auto chunkSizeInKB = (chunkSize)/KB;
	str C;	
	uint64_t numBuckets = STOR->buckets->accounting->getBucketsInUse().size();
	uint64_t numMetaBuckets = STOR->metaBuckets->accounting->getBucketsInUse().size();
	uint64_t hashesInMem = 0;
	std::map<str,int64_t> hashDistribution;
	uint64_t numDedupKbs = 0;
	auto hashesIndex = STOR->buckets->hashesIndex.list();
	for(auto hsh: hashesIndex) {
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
	auto bucketList = STOR->metaBuckets->loaded.list();
	for(auto b: bucketList) {
		for(unsigned a=0;a<chunksInBucket;a++) {
			auto C = b->getChunk(a);
			auto type = C->as<inode_header_only>()->header.type;
			switch(type) {
				case inode_type::NODE:
					++numINodes;
					{
						switch(C->as<inode>()->mode.type()) {
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
	C+= BUILDSTRING("(Dsk) Hashes: ", STOR->buckets->hashesIndex.size()," * ",chunkSizeInKB,"KB == ",(STOR->buckets->hashesIndex.size()*chunkSizeInKB)/KB,"MB\n");
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
	return STOR->metaBuckets->loaded.size() * chunksInBucket * chunkSize;
}

my_off_t fs::readMetadata(unsigned char* buf, my_size_t size, my_off_t offset) {
	lckguard l(_mut);
	//my_size_t firstBucket = offset/(chunksInBucket * chunkSize);
	my_size_t bucketIdx = 0;

	my_off_t offsetInBuf = 0;
	my_off_t actualOffset = 0;
	auto metaBucketList = STOR->metaBuckets->loaded.list();
	for(auto it: metaBucketList) {
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
	
	return std::make_pair<uint64_t,uint64_t>((uint64_t)chunkSize, STOR->buckets->hashesIndex.size()+ STOR->metaBuckets->hashesIndex.size());
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


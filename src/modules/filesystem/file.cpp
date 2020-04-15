// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

/**
 * file.cpp contains the filesystem::file class, this class governs the concept of a file and/or a directory
 * It translates reads/writes and other file operations into changes to the hashes and chunks that make up the file.
 */
 
#include "file.h"
#include "fs.h"
#include "storage.h"
#include "journal.h"
#include "hash.h"
#include "chunk.h"
#include "bucket.h"
#include "main.h"
#include "mode.h"
#include "modules/util/str.h"
#ifndef _WIN32
#include "unistd.h"
#include <fcntl.h>
#else
#include <sys/stat.h>
#endif

/*
 IDEA: Should make a template that governs std::vector<std::shared_ptr<>> so that we very little locking:
		Methods: getRange() //retrieve a range from the list atomicly.
		         updateRange() //Atomicly swap the range
		         expand() // Add new items to the back of the list.
		         shrinkAndReturn() //Remove X items from the back of the list and return them.
 */
		
#include <mutex>

using namespace filesystem;
using namespace script::SLT;

file::file(specialFile intype):  extraMeta(script::make_json()), metaChunk(chunk::newChunk(0,nullptr)){
	_type = intype;
	refs.store(0); 
	isDeleted.store(false);
	numHashWrites.store(0); 
	numHashReads.store(0);
	loadHashes();
}

file::file(std::shared_ptr<chunk> imeta, const str & ipath, const std::vector<permission> & ipathPerm) : metaChunk(imeta), path(ipath),pathPermissions(ipathPerm),hashList() { 
	extraMeta = script::make_json();
	loadHashes();
	refs.store(0); 
	isDeleted.store(false);
	numHashWrites.store(0); 
	numHashReads.store(0);
	
	_ASSERT(INode()->nlinks.is_lock_free());
	
	FS->srvDEBUG("Created fileHandle ",(uint64_t)this," for ",path);
};
file::~file() {
	rest();
	FS->srvDEBUG("Closed fileHandle ",(uint64_t)this," for ",path);
	if(refs!=0) {
		FS->srvERROR(path," file was mishandled! refcount: ",refs.load());
	}
	
};




std::vector<bucketIndex_t> filesystem::file::setDeletedAndReturnAllUsedInodes() {
	isDeleted.store(true); 
	lckunique l(_mut);//Locking required for INode list
	std::vector<bucketIndex_t> ret;
	ret.push_back(INode()->myID);
	
	if(INode()->nextID) {
		auto c = FS->inoToChunk(INode()->nextID);
		while(c) {
			ret.push_back(c->as<inode_ctd>()->myID);
			auto next = c->as<inode_ctd>()->nextID;
			if(next) {
				c = FS->inoToChunk(next);
			} else {
				c = nullptr;
			}
		}
	}
	if(INode()->metaID) {
		auto c = FS->inoToChunk(INode()->metaID);
		while(c) {
			ret.push_back(c->as<inode_ctd>()->myID);
			auto next = c->as<inode_ctd>()->nextID;
			if(next) {
				c = FS->inoToChunk(next);
			} else {
				c = nullptr;
			}
		}
	}
	
	FS->srvDEBUG("Set ",path," isDeleted flag");
	return ret;
}



void file::storeMetaProperties(void) {
	if(!valid())return;
	lckunique l(_mut); // Need lock to protect extraMeta variable
	
	if(INode()->myID) {
		str metaString = extraMeta->serialize(0);
		if(metaString.size()>2) {
			FS->srvDEBUG("t_file::storeMetaData: ", path, ":", metaString.size());
			FS->storeMetaDataInINode(INode(),metaString);
		}
	}

}

void file::setMetaProperty(const str & propertyname,const std::set<uint64_t> & in) {
	if(!valid())return;
	lckunique l(_mut); // Need lock to protect extraMeta variable
	extraMeta->clearProperty(propertyname);
	auto *arr = &extraMeta->get<I>(propertyname,0,in.size());
	auto idx = 0;
	for(auto & i:in) {
		arr[idx] = i;++idx;
	}
}


void file::setMetaProperty(const str & propertyname,script::JSONPtr in) {
	if(!valid())return;
	lckunique l(_mut); // Need lock to protect extraMeta variable
	extraMeta->set(propertyname,in);
}




void file::addLink(void) { ++INode()->nlinks; }
void file::removeLink(void) { --INode()->nlinks; } 

bool file::requireType(fileType required) {
	if(type()!=required) {
		FS->srvERROR(path,": required type: ",(int)required," current type: ",(int)type());
		return false;
	}
	return true;
}



fileType file::type() {
	if(_type == specialFile::ERROR)return fileType::INVALID;
	
	switch(INode()->mode.type()) {
		case mode::TYPE_DIR: return fileType::DIR;
		case mode::TYPE_REG: return fileType::FILE;
		case mode::TYPE_LNK: return fileType::LNK;
		case mode::TYPE_FIFO: return fileType::FIFO;
	}

	return fileType::INVALID;
}


my_uid_t file::uid() {
	return INode()->uid;
	
}
my_gid_t file::gid() {
	return INode()->gid;
}

my_size_t file::size() {
	if(_type==specialFile::METADATA) {
		return FS->metadataSize();
	} else if(_type==specialFile::STATS) {
		return FS->getStats().size();
	}
	return INode()->size;
}
my_mode_t filesystem::file::mode() {
	return INode()->mode;
}

my_ino_t filesystem::file::ino() {
	return INode()->myID;
}

timeHolder file::mtime() {
	return INode()->mtime;
}
timeHolder file::atime() {
	return INode()->atime;
}
timeHolder file::ctime() {
	return INode()->ctime;
}

my_err_t file::chmod(my_mode_t mod,const context * ctx) {
	if(!valid()) {
		return EE::entity_not_found;
	}
	bool fileOwned = ctx->uid == uid();
	

	if(!fileOwned) {
		if(!validate_access(ctx,access::W)) {
			return EE::permission_denied;
		}
		if(!validate_ownership(ctx,mod)) {
			return EE::permission_denied;
		}
	}
	
	if(ctx->gid!=gid() && ctx->uid!=0) {
		mod |=mode::FLAG_SGID;
		mod ^=mode::FLAG_SGID;
	}
	auto tv = currentTime();
	INode()->mtime = tv;
	INode()->ctime = tv;
	_ASSERT((mod & mode::TYPE) == (INode()->mode.type()));
	INode()->mode = mod;
	
	
	return EE::ok;
}

my_err_t file::chown(my_uid_t uid, my_gid_t gid, const context * ctx) {
	if(!valid()) {
		return EE::entity_not_found;
	}

	/*
	 * Only  a  privileged  process  (Linux: one with the CAP_CHOWN capability) may change the owner of a file.  The owner of a file may change the group of the file to any group of which that owner is a member.  A privileged process (Linux: with
	 *     CAP_CHOWN) may change the group arbitrarily.
	 * 
	 *     If the owner or group is specified as -1, then that ID is not changed.
	 * 
	 *     When the owner or group of an executable file is changed by an unprivileged user, the S_ISUID and S_ISGID mode bits are cleared.  POSIX does not specify whether this also should happen when root does the chown(); the Linux behavior depends
	 *     on  the kernel version, and since Linux 2.2.13, root is treated like other users.  In case of a non-group-executable file (i.e., one for which the S_IXGRP bit is not set) the S_ISGID bit indicates mandatory locking, and is not cleared by a
	 *     chown().
	 * 
	 *     When the owner or group of an executable file is changed (by any user), all capability sets for the file are cleared.
	 * 
	 */
	
	if(!validate_access(ctx,access::NONE)) {
		return EE::access_denied;
	}
	constexpr my_uid_t maxUid = std::numeric_limits<my_uid_t>::max();
	constexpr my_gid_t maxGid = std::numeric_limits<my_gid_t>::max();
	//auto tv = currentTime();
		
	if(ctx->uid==0) {
		//Root may change arbitrarily
	} else {
		if(uid!=file::uid() && uid!=maxUid) {//User cannot change uid
			FS->srvDEBUG("User ",ctx->uid, " tryed to switch ",file::uid()," to ",uid);
			return EE::permission_denied;
		}
		if(file::uid()!=ctx->uid) {
			//I dont own this:
			return EE::permission_denied;
		}
		//I am not a member:
		if(ctx->gids.find(gid)== ctx->gids.end() && ctx->gid!=gid && gid != maxGid){
			//FS->srvERROR(" gid not in list: ",ctx->gids);
			return EE::permission_denied;
		}
		


	}
	if(uid!=maxUid || gid != maxGid) {
		INode()->mode.clear(mode::FLAG_SGID|mode::FLAG_SUID);
	}



	auto tv = currentTime();

	CLOG("chown:: update timestamp for ",path);
	INode()->mtime = tv;
	INode()->ctime = tv;

	if(uid!=maxUid)INode()->uid = uid;
	if(gid!=maxGid)INode()->gid = gid;
	return EE::ok;


}

my_err_t file::addNode(const str & name,shared_ptr<chunk> nodeMeta,bool force,const context * ctx,std::shared_ptr<journalEntry> je) {
	if(!valid()) {
		return EE::entity_not_found;
	}
	if(!validate_access(ctx,access::WX)) {
		return EE::access_denied;
	}
	lckunique l(_mut);
	auto directory = script::make_json();
	if(!readDirectoryContent(directory)) {
		return EE::permission_denied;
	}
	
	if(directory->hasProperty(name)) {
		return EE::exists;
	}
	
	//if we overwrite a node here, we throw away an inode.
	_ASSERT(nodeMeta->as<inode>()->myID);
	
	FS->srvDEBUG("Adding node ",name," to ",path," ino: ",nodeMeta->as<inode>()->myID, " mode:",nodeMeta->as<inode>()->mode.load());
	(*directory)[name] = (uint64_t)nodeMeta->as<inode>()->myID;
	if(!writeDirectoryContent(directory,je)) {
		return EE::io_error;
	}
	INode()->ctime = currentTime();
	
	
	return EE::ok;
}
my_err_t file::removeNode(const str & name,const context * ctx,std::shared_ptr<journalEntry> je) {
	if(!valid()) {
		return EE::entity_not_found;
	}
	if(!validate_access(ctx,access::WX)) {
		return EE::access_denied;
	}
	lckunique l(_mut);
	auto directory = script::make_json();
	if(!readDirectoryContent(directory)) {
		return EE::permission_denied;
	}
	
	if(!directory->hasProperty(name)) {
		return EE::entity_not_found;
	}
	
	if(directory->clearProperty(name)!=1) {
		return EE::entity_not_found;
	}
	
	if(!writeDirectoryContent(directory,je)) {
		return EE::io_error;
	}
	INode()->ctime = currentTime();
	
	return EE::ok;
}
my_err_t file::hasNode(const str & name,const context * ctx,bucketIndex_t * id) {
	if(!valid()) {
		return EE::entity_not_found;
	}
	if(ctx && !validate_access(ctx,access::X)) {
		return EE::access_denied;
	}
	
	auto directory = script::make_json();
	if(!readDirectoryContent(directory)) {
		return EE::permission_denied;
	}
	
	if(!directory->hasProperty(name)) {
		return EE::entity_not_found;
	}
	if(id) {
		*id = directory->get<I>(name);
	}
	
	return EE::ok;
}

bool file::setTimes(const timeHolder tv[2]) {
	if(valid()) {
		INode()->atime = tv[0];
		INode()->mtime = tv[1];
		return true;
	}
	return false;
}
void file::loadStat(fileType * T,my_mode_t * M, my_off_t * S,my_gid_t * G,my_uid_t * U,timeHolder * at,timeHolder * mt,timeHolder * ct,my_ino_t * in) {
	*T = type();
	*M =  mode();
	*S = size(); 
	*G = gid();
	*U = uid();
	*mt = mtime();
	*at = atime();
	*ct = ctime();
	*in = ino();
}

void file::loadHashes(void) {
	//No locking required as it is only called from constructor.
	if(type()==fileType::FILE || type()==fileType::DIR || type()==fileType::LNK) {
		auto numHashes = size()/chunkSize;
		if(size()%chunkSize>0) {
			++numHashes;
		}
		hashList.truncateAndReturn(numHashes,FS->zeroHash()).size();
		auto imax = inode::numctd;
		uint64_t idx = 0;
		auto c = metaChunk;
		auto nextId = c->as<inode>()->nextID;
		_listType hashes;
		while(numHashes>0) {
			bucketIndex_t i;
			if(imax==inode::numctd) {
				//We are loading from an inode
				i = c->as<inode>()->ctd[idx];
			} else {
				//We are loading from an inode_ctd
				i = c->as<inode_ctd>()->ctd[idx];
			}
			if(i) {
				auto loadHash = STOR->getHash(i);
				if(loadHash) {
					hashes.push_back(loadHash);
				} else {
					FS->srvWARNING("Missing hash ",hashes.size()," for loadHashes of ",path);
					hashes.push_back(FS->zeroHash());
					hashes.back()->incRefCnt();	
				}
			} else {
				break;
			}
			--numHashes;++idx;
			if(idx>=imax) {
				imax = inode_ctd::numctd;
				idx=0;
				if(nextId) {
					c = FS->inoToChunk(nextId);
					nextId = c->as<inode_ctd>()->nextID;
				} else {
					break;
				}
			}
		}
		if(numHashes) {
			FS->srvWARNING(numHashes," missing hashes for file:",path);
			FS->zeroHash()->incRefCnt(numHashes);
		}
		_ASSERT(hashList.updateRange(0,hashes)==true);
	}
	
	
	if(INode()->metasize>0) {
		FS->srvDEBUG("loading extra metadata for node: ",path);
		str metaString = FS->loadMetaDataFromINode(INode());
		extraMeta->unserialize(metaString);
	}
	
	
}



void file::updateTimesWith(bool A,bool C, bool M,const timeHolder & t) {
	if(A) INode()->atime=t;
	if(C) INode()->ctime=t;
	if(M) INode()->mtime=t;
}


my_err_t file::truncate(my_off_t newSize) {
	if(!valid()) {
		return EE::entity_not_found;
	}
	
	//No locking required as it only updates atomic metaData & works with the atomic hashList.
	if(_type!=specialFile::REGULAR) return EE::entity_not_found;
	
	if(newSize > maxFileSize) {
		return EE::too_big;
	}
	
	lckunique l(_mut);//@todo: test locking for this operation, somehow truncating a file can change the md5 of another file!!
	
	FS->srvDEBUG("file::truncate: ", path, " to ", newSize);
	std::vector<hashPtr> toDelete;
	timeHolder tv = currentTime();

	FS->srvDEBUG("update timestamp for ",path);
	if(newSize!=(my_off_t)size()) {
		INode()->mtime = tv;
		INode()->ctime = tv;
	}

	my_size_t newNum = newSize/chunkSize,expected = 0;
	if(newSize % chunkSize>0) {
		++newNum;
	}
	
	if(newNum< hashList.getSize()) {
		expected = hashList.getSize()-newNum;
		toDelete = hashList.truncateAndReturn(newNum,FS->zeroHash());
		_ASSERT(toDelete.size()==expected);
		for (auto deleteHash : toDelete) {
			if(deleteHash ) {
				--expected;
				deleteHash->decRefCnt();
				deleteHash->rest(); //This will store the hash, or, will delete it altogether.
			}
		}
		toDelete.clear();//This will call rest for all hashes that are exclusivly owned by this
		_ASSERT(expected==0);
	} else if(newNum>hashList.getSize()) {
		try{
			auto numAdded = hashList.truncateAndReturn(newNum,FS->zeroHash()).size();
			FS->zeroHash()->incRefCnt(numAdded);
		} catch( std::exception & e) {
			return EE::too_big;
		}
	}
	
	
	
	INode()->size = newSize;
	rest();
	return EE::ok;
}

my_off_t file::read(unsigned char * buf,my_size_t size,const my_off_t offset) {
	if(!valid()) {
		return 0;
	}
	_ASSERT(buf != nullptr);
	FS->_readStats.at(fs::classifySize(size))++;
	//No locking required as this call only reads from atomic members.
	if(_type==specialFile::METADATA) {
		return FS->readMetadata(buf,size,offset);
	} else if(_type==specialFile::STATS) {
		//CLOG("read stats ",size," ",offset);
		auto c = FS->getStats();
		if((unsigned)offset<c.size()) {
			std::copy(c.begin()+offset,std::min(c.end(),c.begin()+offset+size),buf);
			return std::min(size, (my_size_t)(c.size()-offset));
		}
		return 0;
	}
	const auto originalSize = size;
	my_off_t offsetInBuf = 0;
	const my_off_t firstHash = offset / chunkSize;
	my_off_t myFileOffset = firstHash * chunkSize;
	auto & fileSize = INode()->size;
	
	my_size_t maxReadSize = std::min(fileSize -offset, size);
	if (offset >= (my_off_t)fileSize) {
		return 0;
	}
	if(size > maxReadSize) {
		size = maxReadSize;
	}
	
	my_size_t numHashesInRead = maxReadSize/chunkSize;
	if(maxReadSize % chunkSize>0) {
		++numHashesInRead;
	}
	try{
		lckshared l2(_mut,std::defer_lock);//, and a shared lock for aligned writes.
		if(_mut.hasUniqueLock()==false) {
			l2.lock();
		}
		
		if (firstHash + numHashesInRead > hashList.getSize()) {
			FS->srvWARNING("Trying to read too many hashes");
			numHashesInRead = hashList.getSize() - firstHash;
		}
		if (numHashesInRead > 0) {
			auto hashes = hashList.getRange(firstHash, numHashesInRead);//optimize this by only fetching the actual required hashes.
			for (auto readHash : hashes) {
				_ASSERT(readHash != nullptr);
				if (offset < myFileOffset + chunkSize && size > 0) {
					auto newOffset = std::max(offset - myFileOffset, (my_off_t)0);//Determine the offset to start the read from.
					auto newSize = std::min(size, (my_size_t)chunkSize - newOffset);
					_ASSERT(newSize <= size);
					readHash->read(newOffset, newSize, &buf[offsetInBuf]);
					offsetInBuf += newSize;
					size -= newSize;
					++numHashReads;
				}
				myFileOffset += chunkSize;
			}
		}
	} catch(std::exception & e) {
		FS->srvERROR("t_file::read: ERROR ",e.what());
		return 0;
	}
	
	if(offsetInBuf!=(my_off_t)maxReadSize) {
		FS->srvWARNING(" read call offsetInBuf!=maxReadSize! (",offsetInBuf,",",maxReadSize,") :",path," originalSize:",originalSize," offset:",offset);
	}
	
	if (numHashReads.load() > (script::int_t)(chunksInBucket * 5)) {
		numHashReads.store(0);
		if (rest()) {
			FS->unloadBuckets();
		}
	}
		
	return offsetInBuf;
}
my_off_t file::write(const unsigned char * buf,my_size_t size, const my_off_t offset) {
	if(!valid()) {
		return 0;
	}
	if(_type!=specialFile::REGULAR) return 0;	
	
	const str dta(reinterpret_cast<const char *>(buf),size);
	
	auto je = JOURNAL->add(journalEntryType::write,bucketIndex_t(),INode()->myID,bucketIndex_t(),0,offset,"",dta);
	
	return writeInner(buf,size,offset,je);
}

my_off_t file::writeInner(const unsigned char * buf,my_size_t size, const my_off_t offset,shared_ptr<journalEntry> je) {

	//CLOG("t_file::write: ",path," ",size," ",offset);

	FS->_writeStats.at(fs::classifySize(size))++;
	auto originalSize = size;
	my_off_t offsetInBuf = 0;
	
	const my_off_t firstHash = offset / chunkSize;
	my_off_t myFileOffset = firstHash * chunkSize;
	
	my_size_t numHashesInWrite = size/chunkSize;
	if(size % chunkSize>0) {
		++numHashesInWrite;
	}
	
	auto & fileSize = INode()->size;
	if(fileSize<offset+originalSize) {
		fileSize = offset+originalSize;
	}
	auto numHashes = fileSize/chunkSize;
	if(fileSize % chunkSize >0) {
		++numHashes;
	}
	if(numHashes!=hashList.getSize()) {
		_ASSERT(hashList.getSize()<numHashes);
		auto addedChunks = hashList.truncateAndReturn(numHashes,FS->zeroHash()).size();
		FS->zeroHash()->incRefCnt(addedChunks);
	}
	
	try{
		lckunique l(_mut,std::defer_lock);//unique lock for unaligned offset writes.
		lckshared l2(_mut,std::defer_lock);//, and a shared lock for aligned writes.
		if(offset % chunkSize >0 ){
			FS->srvWARNING("Hitting slow path (unaligned write) for write to: ",path);
			l.lock();
		} else if(_mut.hasUniqueLock()==false) { //Do not try to lock shared if we already posses a unique lock.
			l2.lock(); 
		}
		
		auto hashes = hashList.getRange(firstHash,numHashesInWrite);
		std::vector<shared_ptr<hash>> hashesToRemoveFromFile;
		std::set<uint64_t> bucketsAffected;
		for (auto & writeHash: hashes) {
			_ASSERT(writeHash!=nullptr);
			if(offset < myFileOffset+ chunkSize && size > 0) {
				auto newOffset = std::max(offset - myFileOffset, (my_off_t)0);//Determine the offset to start the read from.
				auto newSize = std::min(size,(my_size_t)chunkSize-newOffset);
				_ASSERT(newSize<=size);
				auto newHsh = writeHash->write(newOffset,newSize,&buf[offsetInBuf]);
				offsetInBuf += newSize;
				size -= newSize;
				if(newHsh) {
					hashesToRemoveFromFile.push_back(writeHash);
					newHsh->incRefCnt();
					bucketsAffected.insert(newHsh->getBucketIndex().bucket());
					bucketsAffected.insert(writeHash->getBucketIndex().bucket());
					writeHash = newHsh;
				}
				//_ASSERT(writeHash->getRefCnt()>0);
			}
			myFileOffset += chunkSize;
		}
		_ASSERT(size==0);
		if(hashesToRemoveFromFile.empty()==false) {
			FS->srvDEBUG("file::write: updating ",hashesToRemoveFromFile.size()," hashes");
			numHashWrites += hashesToRemoveFromFile.size(); //Keep track of hash writes to know when to rest the file
			_ASSERT(hashList.updateRange(firstHash,hashes)==true); //Update the hashes in this write with the new versions
			for(auto i: hashesToRemoveFromFile) {
				i->decRefCnt();
			}
			for(auto i:bucketsAffected) { //Store a reference to the journalEntry in the affected buckets
				STOR->buckets->getBucket(i)->addChange(je);
			}
		}
		INode()->mtime = currentTime();
	} catch(std::exception & e) {
		FS->srvERROR("file::write: ERROR: ",e.what());
		return 0;
	}
	
	if(offsetInBuf!=(my_off_t)originalSize) {
		FS->srvWARNING("file::write: offsetInBuf!=originalSize (",offsetInBuf,",",originalSize,") :",path," originalSize:",originalSize," offset:",offset);
	}
	
	if (numHashWrites.load() > (script::int_t)(chunksInBucket * 5)) {
		numHashWrites.store(0);
		if (rest()) {
			FS->unloadBuckets();
		}
	}

	return offsetInBuf;
}


bool file::readlnk(char * buffer, my_size_t bufferSize) {
	if(!valid()) {
		return false;
	}
	//No locking required as this only reads atomic metadata
	if(_type!=specialFile::REGULAR) return false;
	if(type()!=fileType::LNK) {
		FS->srvERROR("READLNK FAIL: NOT A LINK");
		return false;
	}
	
	for(unsigned a=0;a<bufferSize;a++)buffer[a]=0;
	if(size()) {
		_ASSERT(buffer!=nullptr);
		auto ret = read(reinterpret_cast<uint8_t*>(buffer),std::min(size(),bufferSize),0);
		if(ret!=(int)size()) {
			FS->srvERROR("READLNK FAIL: size()!=read: ",size(),"!=",ret," bufferSize:",bufferSize," buffer:",buffer);
			return false;
		}
	}
	return true;
}


bool file::isFullDir() {
	if(!valid()) {
		return false;
	}
	if(type()!=fileType::DIR) return false;

	auto directory = script::make_json();
	if(!readDirectoryContent(directory)) {
		return true;
	}

	for(auto & v:*directory) {
		if(v.first.empty()==false && v.first.at(0)!='/') {
			return true;
		}
	}
	return false;
}

bool file::swapContent(const str & newContent,std::shared_ptr<journalEntry> je) {
	if(!valid()) {
		return false;
	}
	if(_type!=specialFile::REGULAR) return false;
	
	lckunique lck(_mut); //Will replace all content, will need a lock.
	
	
	std::set<uint64_t> affectedBuckets;
	std::vector<shared_ptr<hash>> newList;
	size_t offset = 0;
	while(offset<newContent.size()) {
		auto newSize=std::min((size_t)chunkSize,(size_t)(newContent.size()-offset));
		auto newHsh = FS->zeroHash()->write(0,newSize,reinterpret_cast<const uint8_t *>(&newContent[offset]));
		_ASSERT(newHsh!=nullptr);
		affectedBuckets.insert(newHsh->getBucketIndex().bucket());
		newHsh->incRefCnt();
		newList.push_back(newHsh);
		offset += newSize;
	}
	
	hashList.swap(newList);
	
	INode()->size = newContent.size();
	INode()->mtime = currentTime();
	INode()->ctime = INode()->mtime = currentTime();
	
	
	for(auto & i: newList) {
		i->decRefCnt();
	}
	
	for(auto b: affectedBuckets) {
		STOR->buckets->getBucket(b)->addChange(je);
	}
	
	return true;
}


//This puts the file + it's data to rest so that it gets stored to hard disk.
bool file::rest() {
	if(!valid()) {
		return false;
	}
	//No locking required as only calls are made to properly protected member functions (perhaps not?)
	if (_type != specialFile::REGULAR) return false;
	lckunique l(_mut); // This operation should not run in paralel. 
	size_t num = 0;
	size_t newMetaChunks = 0;
	auto hashes = hashList.getAll();
	if(hashes.empty()==false) {
		unsigned idx = 0;
		auto imax = inode::numctd;
		auto c = metaChunk;
		auto nextId = c->as<inode>()->nextID;	
		for(auto & H:hashes) {
			_ASSERT(H!=nullptr); //this HAPPENS, need to account for having empty hashes
			if(H->getRefCnt()<=0) {
				FS->srvERROR("file::rest is writing a reference to deleted hash: ",H->getHashStr(),H->getBucketIndex());
			}
			if(H->rest()) {
				++num;
			}
			//_ASSERT(H->getRefCnt()>0);
			if(imax==inode::numctd) {
				//We are loading from an inode
				c->as<inode>()->ctd[idx] = H->getBucketIndex();
			} else {
				c->as<inode_ctd>()->ctd[idx] = H->getBucketIndex();
			}
			
			++idx;
			if(idx>=imax) {
				FS->storeInode(c);
				if(nextId) {
					c = FS->inoToChunk(nextId);
				} else {
					if(imax == inode::numctd) {
						c = FS->createCtd(c->as<inode>(),false);
					} else {
						c = FS->createCtd(c->as<inode_ctd>());
					}
					++newMetaChunks;
				}
				nextId = c->as<inode_ctd>()->nextID;
				imax = inode_ctd::numctd;
				idx=0;
			}
		}
		
		//Make sure last inode_ctd is properly stored (better once to many then too little.)
		FS->storeInode(c);
	} else {
		//Make sure we truncate the inode list, so loadHashes does not accidently load data from other files.
		INode()->ctd[0] = 0;
	}
	
	FS->srvDEBUG("file::rest ",num,"/",hashes.size()," hashes for ", path," with ",newMetaChunks," new metaChunks");
	
	if(INode()->myID) {
		FS->storeInode(metaChunk);
	}
	
	
	return num>0;
}


bool file::validate_access(const context * ctx,access da,access dda,bool checkStickyOwner) {
	if(!valid()) {
		return false;
	}
	
	if(ctx->uid==0) return true;
	//No locking as pathPermissions is static data once file is instantiated.
	bool stickySet = false;
	bool owndir = false;
	for(const auto & p:pathPermissions) {
		const auto u = p.uid == ctx->uid; 
		const auto g = p.gid == ctx->gid;
		const auto m = p.mode;
#ifndef _WIN32
		if(checkStickyOwner == true && stickySet == false) {
			if((m&mode::FLAG_SVTX) >0) {
				if(u ) {
					owndir = true;
				}
				stickySet = true;
			}
		}
#endif		
		if(u) {
			if((m & 0b001000000)>0) continue;
			return false;
		}
		if(g) {
			if((m & 0b000001000)>0) continue;
			return false;
		}
		if((m & 0b000000001)>0) continue;
		return false;
	}
	if(dda!=access::NONE) {
		if(pathPermissions.empty()==false) {

			const auto u = pathPermissions.back().uid == ctx->uid; 
			const auto g = pathPermissions.back().gid == ctx->gid;
			const auto m = pathPermissions.back().mode;

			//CLOG("Checking access to folder: ",m," :",(int)dda," sticky:",stickySet);
			if(u) {
				my_mode_t desired = (my_mode_t)dda << 6;
				if((m & desired) == desired) {} else { return false; }
			}else if(g) {
				my_mode_t desired = (my_mode_t)dda << 3;
				if((m & desired) == desired) {} else { return false; }
			} else {
				my_mode_t desired = (my_mode_t)dda ;
				if((m & desired) == desired) {} else { return false; }
			}
		}
	}

	const auto u = uid() == ctx->uid; 
	const auto g = gid() == ctx->gid;
	const auto m = mode();


	if(checkStickyOwner && stickySet ) {
		if(u==false  && owndir ==false) {
			return false;
		} 
	}

	if(u) {
		my_mode_t desired = (my_mode_t)da << 6;
		if((m & desired) == desired) { return true; }
		return false;
	}
	if(g) {
		my_mode_t desired = (my_mode_t)da << 3;
		if((m & desired) == desired) { return true; }
		return false;
	}

	my_mode_t desired = (my_mode_t)da;
	if((m & desired) == desired) { return true; }

	return false;
}

bool file::validate_ownership(const context * ctx,my_mode_t newMode) {
	if(!valid()) {
		return false;
	}
	if(ctx->uid==0) {
		return true;
	}
	
	auto m = mode();
	bool doIhHaveOwnerShip = (ctx->uid == uid()) || (ctx->gid == gid());
	bool doesNewModeChangeOwnerShip = !((m&0b111111111)==(newMode&0b111111111));
	if(doIhHaveOwnerShip==false && doesNewModeChangeOwnerShip == true) {
		FS->srvDEBUG("Ownership == false && doesChangeOwnerShip");
		return false;
	}
	return true;
	
}


bool file::readDirectoryContent(script::JSONPtr out) {
	if(!valid()) {
		return false;
	}
	
	if(type()==fileType::DIR) {
		if(size()) {
			str content;
			content.resize(INode()->size);
			if(read(reinterpret_cast<uint8_t*>(&content[0]),content.size(),0)!=(int)content.size()) {
				FS->srvERROR("Failed to read directory:",path);
				return false;
			}
			try{
				out->unserialize(content);
			} catch(std::exception & e) {
				FS->srvERROR("parse dir failed:",path," e: ",e.what()," c:<<",content,">> csize:",content.size());
				return false;
			}
		}
	}
	return true;
}
bool file::writeDirectoryContent (script::JSONPtr in,std::shared_ptr<journalEntry> je) {
	if(!valid()) {
		return false;
	}
	
	
	auto content = in->serialize(0);
	if(content.size()==2) {
		content.clear();
	}
	
	_ASSERT(swapContent(content,je)==true);
	rest(); 
	
	return true;
}

void file::open(void) {
	//No locking, refs == atomic
	++refs;
	FS->srvDEBUG("t_file::open ",path, " ",refs.load());
}
bool file::close(void) {
	//No locking, refs == atomic
	FS->srvDEBUG("t_file::close ",path);
	--refs;
	if(_type==specialFile::REGULAR) { //Rest the file on close EVERY time it closes, not just when the last ref is let go.
		if(rest()) {
			FS->unloadBuckets();
			return true;
		}
	}
	return false;
}


my_err_t file::replayEntry(const journalEntry * entry, const str & name, const str & data,const context * ctx,shared_ptr<journalEntry> je) {
	if(!valid())return EE::entity_not_found;
	
	switch(entry->type) {
		case journalEntryType::write:
			return writeInner(_STRTOBYTESIZE(data),entry->offset,nullptr) == (my_off_t)entry->dataLength ? EE::ok : EE::io_error;
			break;
		default:
			return EE::invalid_syscall;
	}
}


void file::setFileDefaults(std::shared_ptr<chunk> meta,my_mode_t mod,const context * ctx) {

	if(ctx) {
		//CLOG("Set file defaults ",ctx->uid," ",ctx->gid);
		meta->as<inode>()->uid = ctx->uid;
		meta->as<inode>()->gid = ctx->gid;
	} else {
		meta->as<inode>()->uid = getuid();
		meta->as<inode>()->gid = getgid();
	}

	meta->as<inode>()->mode = mod;
	
	timeHolder tv = currentTime();
	meta->as<inode>()->mtime=tv;
	meta->as<inode>()->atime=tv;
	meta->as<inode>()->ctime=tv;
	
	meta->as<inode>()->nlinks = 1;
}
/*void file::setDirMode(std::shared_ptr<chunk> meta,my_mode_t m) {
	_ASSERT((m&mode::TYPE)==0);
	meta->as<inode>()->mode = m | mode::TYPE_DIR;	
}
void file::setFileMode(std::shared_ptr<chunk> meta,my_mode_t m, my_dev_t dev)  {
	_ASSERT((m&mode::TYPE)!=0);

	meta->as<inode>()->mode = m;
}*/


// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
/**
 * The class bucketaccounting is responsible for tracking free bucketIndex's
 * Most operation are atomic. Will also maintain a <set> of used buckets.
 */
#define __FILESYSTEM_BUCKETACCOUNTING_CPP
#include "bucketaccounting.h"
#include "storage.h"

#include <algorithm>
using namespace filesystem;


//@todo: idea: could have a changes_since_get system to let the user known when to save metadata.

bucketaccounting::bucketaccounting(std::set<uint64_t> _buckets,bucketInfo * iinfo) :  hint(1),bucketsInUse(_buckets),info(iinfo) { 
	_ASSERT(freeList.is_lock_free());
	_ASSERT(availableList.is_lock_free());
	//

	availableList = nullptr;
	freeList = &_records[0];
	for(auto & i: _records) {
		i.data.fullindex = 0;
	}
	cleanRecords();
}


void bucketaccounting::insertIntoList(accountingNode * newNode,atomicNodePtr & list) {
	while(true) {
		auto * expected = list.load();
		newNode->nextNode = expected;
		if(list.compare_exchange_strong(expected,newNode)) {
			break;
		}
	}
}


accountingNode * bucketaccounting::takeFromList(atomicNodePtr & list) {
	auto * expected = list.load();
	while(expected) {
		auto * newNode = expected->nextNode.load();
		if(list.compare_exchange_strong(expected,newNode)) {
			return expected;
		}
	} 
	return nullptr;
}

accountingNode * bucketaccounting::stealList(atomicNodePtr & list) {
	auto * expected = list.load();
	while(expected) {
		accountingNode * newNode = nullptr;
		if(list.compare_exchange_strong(expected,newNode)) {
			return expected;
		}
	} 
	return nullptr;
}


/*bool bucketaccounting::clearBucket(uint64_t id) {
	bucketIndex_t b;
	std::vector<bucketIndex_t> rollback;
	unsigned numDeleted=0;
	for(auto & i:map) {
		b.fullindex = i.load();
		if(b.bucket==id) {
			auto expected = b.fullindex;
			if(i.compare_exchange_strong(expected,0)) {
				rollback.push_back(b);
				++numDeleted;
				--numAvailable;
			} 
		}
	}
	if(numDeleted!=chunksInBucket) {
		FS->srvWARNING("Rollback bucket changes for ",id);
		for(auto & i:rollback) {
			postInner(i);
		}
		return false;
	}
	return true;

}*/


void bucketaccounting::cleanRecords(void) {
	for(unsigned a=1;a<_records.size();a++) {
		_records[a-1].nextNode = &_records[a];
		_records[a].nextNode = nullptr;
	}
}

void bucketaccounting::createBuckets(void) {
	unsigned recId = 0;
	for(unsigned a=0;a<maxBuckets;a++) {
		while(bucketsInUse.find(hint)!=bucketsInUse.end()) {
			++hint;
		}
		auto myBucketId = hint;
		bucketsInUse.insert(myBucketId);
		++hint;
		for(unsigned b=0;b<chunksInBucket;b++) {
			_records[recId].data.bucket = myBucketId;
			_records[recId].data.index = b;
			++recId;
		}
		info->createNewBucket(myBucketId);
	}
	_ASSERT(recId==_records.size());
	cleanRecords();
	availableList = &_records[0];
	freeList = nullptr;
	
}

void bucketaccounting::releaseBuckets(void) {
	//Take the list of available ID's
	_ASSERT(freeList.load()==nullptr);
	auto * myList = stealList(availableList);
	_ASSERT(myList!=nullptr);
	
	//Count up the ID's per bucket
	std::map<uint64_t,uint32_t> _availableMap;
	for (auto & n:_records){
		_availableMap[n.data.bucket] ++;
	}
	
	//Find the buckets that have 100
	std::set<uint64_t> _toDelete;
	for(auto & i: _availableMap) {
		if(i.second == chunksInBucket) {
			_toDelete.insert(i.first);
		}
	}
	_ASSERT(_toDelete.size()>0); //If we find nothing to delete we crash due to fragmentation!!
	std::vector<accountingNode *> delList,availList;

	for(auto & n:_records) {
		if(n.data.fullindex == 0 || _toDelete.find(n.data.bucket)!=_toDelete.end()) {
			n.data.fullindex = 0;
			delList.push_back(&n);
		} else {
			availList.push_back(&n);
		}
	}
	while(delList.empty()==false) {
		insertIntoList(delList.back(),freeList);
		delList.pop_back();
	}
	while(availList.empty()==false) {
		insertIntoList(availList.back(),availableList);
		availList.pop_back();
	}
	
	for(auto & b:_toDelete) {
		info->removeBucket(b);
		bucketsInUse.erase(b);
	}

	
	_ASSERT(freeList.load()!=nullptr);
}




std::set<uint64_t> bucketaccounting::getBucketsInUse() {
	std::set<uint64_t> ret;
	lckguard l(_mut); 
	ret = bucketsInUse;
	return ret;
}

void bucketaccounting::post(bucketIndex_t data) {
	while(true) {
		auto * node = takeFromList(freeList);
		if(node) {
			node->data = data;
			insertIntoList(node,availableList);
			break;
		} else {
			//no free space to post stuff to. availableList should be filled, and freeList empty.
			{
				lckguard l(_mut);
				if(freeList.load()==nullptr && availableList.load()!=nullptr) {
					//Find and release buckets that are 100% free!
					releaseBuckets();
				}
			}
		}
	}
}




bucketIndex_t bucketaccounting::fetch() {
	bucketIndex_t ret;
	while(true) {
		auto * node = takeFromList(availableList);
		if(node) {
			ret = node->data;
			node->data.fullindex = 0;
			insertIntoList(node,freeList);
			break;
		} else {
			//No more in availableList. freeList should be filled.
			{
				lckguard l(_mut);
				if(availableList.load()==nullptr && freeList.load()!=nullptr) {
					//make new nodes and add them to the list.
					createBuckets();
				}
			}
		}
	}
	return ret;
}



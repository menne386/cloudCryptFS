// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef __FILESYSTEM_BUCKETACCOUNTING_H
#define __FILESYSTEM_BUCKETACCOUNTING_H

#include "types.h"
#include "chunk.h"
#include "hash.h"
#include <atomic>
#include <array>
#include <limits>
#include <map>
#include <set>
#include <locks.h>




namespace filesystem{

class bucketInfo;
class accountingNode;

typedef std::atomic<accountingNode*> atomicNodePtr;

class accountingNode{
public:
	bucketIndex_t data;
	atomicNodePtr nextNode;
};

class bucketaccounting{
	static constexpr unsigned maxBuckets = 10;
	locktype _mut;
	
	
	
	std::array<accountingNode,chunksInBucket*maxBuckets> _records;
	
	atomicNodePtr availableList;
	atomicNodePtr freeList;

	void insertIntoList(accountingNode * newNode,atomicNodePtr & list);
	accountingNode * takeFromList(atomicNodePtr & list);
	accountingNode * stealList(atomicNodePtr & list);
	

	uint64_t hint;
	void cleanRecords(void);
	void createBuckets(void);
	void releaseBuckets(void);
	
	std::set<uint64_t> bucketsInUse;
	bucketInfo * info;
	
	public:
		bucketaccounting(std::set<uint64_t> _buckets,bucketInfo * iinfo); 

	std::set<uint64_t> getBucketsInUse();

	void post(bucketIndex_t data);//Post a single entry back to the pool

	bucketIndex_t fetch();//Get a single entry from the pool.

	

};

};


#endif /* __FILESYSTEM_BUCKETACCOUNTING_H */


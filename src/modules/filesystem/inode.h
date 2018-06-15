// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef FILESYSTEM_INODE_H
#define FILESYSTEM_INODE_H
#pragma once

#include "types.h"
#include "chunk.h"

namespace filesystem { 
	//INode class represents the metadata in binairy form for storage on disk

	enum class inode_type:uint32_t{ 
		NONE=0,NODE=0xFF00FABB,CTD=0xFF00FACC
	};
	
	class inode_header final{
		public:
		union{
			inode_type type;
		};
		uint32_t version;
	};
	static_assert(sizeof(inode_header)==8,"inode header should b 8 bytes");
	static_assert(sizeof(my_gid_t)==4,"sizeof my_gid_t should be 4 bytes");
	static_assert(sizeof(my_uid_t)==4,"sizeof my_uid_t should be 4 bytes");
	static_assert(sizeof(my_mode_t)==4,"sizeof my_mode_t should be 4 bytes");
	static_assert(sizeof(my_size_t)==8,"sizeof my_size_t should be 8 bytes");
	static_assert(sizeof(timeHolder)==16,"sizeof timeHolder should be 16 bytes");
	static_assert(sizeof(bucketIndex_t)==8,"sizeof bucketIndex_t should be 8 bytes");
	
	typedef std::atomic<my_off_t> atomic_off_t;
	typedef std::atomic<my_size_t> atomic_size_t;
	typedef std::atomic<my_uid_t> atomic_uid_t;
	typedef std::atomic<my_gid_t> atomic_gid_t;
	typedef std::atomic<uint32_t> atomic_metasize_t;
	typedef std::atomic<my_mode_t> atomic_mode_t;
	class inode_header_only final{
	public:
		inode_header header;
		uint64_t nothing[(chunkSize-sizeof(inode_header))/sizeof(uint64_t)];
		void assertTypeValid(); 
	};
		
	/**
	 * Inode structure
	 * WARNING: do NOT change the layout of this struct (you will need to create a new version to do that)
	 */
	class inode final{
	private:
		inode_header header;
	public:
		constexpr static unsigned numctd = 495;
		constexpr static uint32_t latestversion = 1;
		constexpr static inode_type mytype = inode_type::NODE;
		bucketIndex_t myID;        //Inode number (my own node ID) 
		bucketIndex_t nextID;      //content is continued on node with this ID
		bucketIndex_t metaID;      //Points to a record holding additional metadata in JSON form.
		atomic_metasize_t metasize;         //metasize in bytes
		atomic_mode_t mode;            //mode flags (determines wether file or folder)
		timeHolder atime;          //access times
		timeHolder ctime;          //change times
		timeHolder mtime;          //mod times
		atomic_off_t nlinks;       //number of links to this node
		atomic_uid_t uid;              //The owner UID
		atomic_gid_t gid;              //The owner GID 
		atomic_size_t size;            //The size in bytes of the file.
		uint64_t latent[3];        //Reserved
		bucketIndex_t ctd[numctd]; //Content for a node (sectors of file content for a file)
		void assertTypeValid(); 
		uint32_t version() { return header.version; }
	};
	
	/**
	 * Inode_ctd structure
	 * WARNING: do NOT change the layout of this struct (you will need to create a new version to do that)
	 */
	class inode_ctd final{
	private:
		inode_header header;
	public:
		constexpr static unsigned numctd = 508;
		constexpr static uint32_t latestversion = 1;
		constexpr static inode_type mytype = inode_type::CTD;
		bucketIndex_t myID;     //my own node ID
		bucketIndex_t nextID;   //link to next node ID
		uint64_t latent; //Reserved for whatever
		union{
			bucketIndex_t ctd[numctd]; //Continued content for the node.
			char charContent[numctd*sizeof(bucketIndex_t)]; //Content for a node
		};
		void assertTypeValid();
		uint32_t version() { return header.version; }
	};
	

	static_assert(sizeof(inode)==chunkSize,"Size of inode is not chunkSize");
	static_assert(sizeof(inode_ctd)==chunkSize,"Size of inode_ctd is not chunkSize");
	
	
	
	
};
/*
 Idea: build up a tree of these inodes on disk, each fits in a single sector.
 The very 1st node == the meta root node. (bucket 1, index 0)          node type inode
 All metadata is stored in metadata buckets protected by the proto encryption key. 
 
 [Root INode]
 
 Startup procedure:
 Read meta bucket 1, instance the root node + metadata to get to the key
 Instance the metadata bucket list from the second entry
 Instance the normal bucket list from the 3rd entry
 
 Load all the hashes from the normal bucket list.
 
 
 
 */



/*
 {
  "files":{
    "._meta":{
      "/atime":[1580912854,767207640],
      "/ctime":[1580912854,767207640],
      "/gid":0,
      "/ino":0,
      "/mode":511,
      "/mtime":[1580912854,767207640],
      "/nlnks":0,
      "/type":1,
      "/uid":0
    },
    "._stats":{
      "/atime":[1580912854,767203967],
      "/ctime":[1580912854,767203967],
      "/gid":0,
      "/ino":0,
      "/mode":511,
      "/mtime":[1580912854,767203967],
      "/nlnks":0,
      "/type":1,
      "/uid":0
    },
    "/atime":[1580912854,766425800],
    "/ctime":[1580912854,766425800],
    "/gid":0,
    "/ino":0,
    "/mode":511,
    "/mtime":[1580912854,766425800],
    "/nlnks":0,
    "/size":0,
    "/type":2,
    "/uid":0,
    "cloudCryptFS.docker":{
      "/ref":"1"
    }
  },
  "settings":{
  },
  "stats":{
    "dirs":0,
    "files":1,
    "nodes":1
  },
  "version":1,
  "x_file_nodes":{
    "1":{
      "/atime":[1580912837,950822789],
      "/ctime":[1580912837,950822789],
      "/dev":0,
      "/gid":0,
      "/hsh":[257,258,259,260,261,262,263,264,265,266,267,268,269,270,271,272,273,274,275,276,277,278,279,280,281,282,283,284,285,286,287,288,289,290,291,292,293,294,295,296,297,298,299,300,301,302,303,304,305,306,307,308,309,310,311,312,313,314,315,316,317,318,319,320,321,322,323,324,325,326,327,328,329,330,331,332,333,334,335,336,337,338,339,340,341,342,343,344,345,346,347,348,349,350,351,352,353,354,355,356,357,358,359,360,361,362,363,364,365,366,367,368],
      "/ino":1,
      "/isref":1,
      "/mode":33261,
      "/mtime":[1580912837,953925002],
      "/nlnks":1,
      "/size":455064,
      "/type":1,
      "/uid":0
    }
  },
  "x_ino":{
    "nextIno":2,
    "reclaimed":0
  },
  "xbuckets":{
    "1":{
      "id":1
    },
    "2":{
      "id":2
    },
    "3":{
      "id":3
    },
    "4":{
      "id":4
    },
    "5":{
      "id":5
    },
    "6":{
      "id":6
    },
    "7":{
      "id":7
    },
    "8":{
      "id":8
    },
    "9":{
      "id":9
    }
  }
}
 */



#endif

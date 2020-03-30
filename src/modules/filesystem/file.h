// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef FILESYSTEM_FILE_H
#define FILESYSTEM_FILE_H

//#include <sys/types.h>
#include "types.h"
#include "inode.h"
#include "modules/script/script_complex.h"
#include "modules/util/atomic_shared_ptr_list.h"
#include <atomic>
#undef ERROR
#include "locks.h"
#include "context.h"

using util::atomic_shared_ptr_list;
namespace filesystem{
	class file;
	class fs;
	class hash;
	
	typedef std::shared_ptr<file> filePtr;
	
	enum class fileType: script::int_t{//Storage for this should be the int_t type of script because the type will be stored in ComplexType
		INVALID = 0,
		FILE = 1,
		DIR = 2,
		LNK=3,
		FIFO=4,
		UNKNOWN = 99,
	};
	
	
	enum class specialFile{
		REGULAR,ERROR,STATS,METADATA
	};

	struct permission{
		my_uid_t uid;
		my_gid_t gid;
		my_mode_t mode;
	};
	
	class file {
	private:
		//friend class fs;
		std::atomic_int numHashWrites,numHashReads;
		std::shared_ptr<script::ComplexType> extraMeta;
		std::shared_ptr<chunk> metaChunk;
		const str path;
		const std::vector<permission> pathPermissions;
		locktypeshared _mut;
		specialFile _type=specialFile::REGULAR;
		
		atomic_shared_ptr_list<hash> hashList;
		typedef atomic_shared_ptr_list<hash>::listType _listType;
		//std::vector<std::shared_ptr<hash>> hashes;
		
		std::atomic_int refs;
		std::atomic_bool isDeleted;
		bool requireType(fileType required);
		void loadHashes(void);
		bool validate_ownership(const context * ctx,my_mode_t newMode);
		inode * INode() {return metaChunk->as<inode>();} 
	public:
		file(specialFile intype);
		file(std::shared_ptr<chunk> imeta, const str & ipath, const std::vector<permission> & ipathPerm);
		~file();
		std::vector<permission> getPathPermissions() {return pathPermissions;};
		bool isSpecial() { return _type!=specialFile::REGULAR; }
		bool valid() {return (_type!=specialFile::ERROR && isDeleted==false);}
		std::shared_ptr<chunk> getMetaChunk() {return metaChunk;}

		bool readDirectoryContent(script::complextypePtr out);
		bool writeDirectoryContent(script::complextypePtr in);
		
		std::vector<bucketIndex_t> setDeletedAndReturnAllUsedInodes();
		void storeMetaProperties(void);
		void setMetaProperty(const str & propertyname,const std::set<uint64_t> & in);
		void setMetaProperty(const str & propertyname,script::complextypePtr in);
		void setSpecialFile(const specialFile in) {_type = in;}
		 
		void addLink(void);
		void removeLink(void);
		auto getNumLinks(void) { return INode()->nlinks.load(); };
		void updateTimesWith(bool A,bool C, bool M,const timeHolder & t);
		
		fileType type();
		
		my_uid_t uid();
		my_gid_t gid();
		my_size_t size();
		my_mode_t mode();
		my_ino_t ino();
		
		int getOpenHandles() const { return refs.load(); }
		
		str getPath() const { return path; }

		timeHolder mtime();
		timeHolder atime();
		timeHolder ctime();
		
		my_err_t chmod(my_mode_t mode, const context * ctx);
		my_err_t chown(my_uid_t uid, my_gid_t gid, const context * ctx);
		
		my_err_t addNode(const str & name,shared_ptr<chunk> nodeMeta,bool force,const context * ctx);
		my_err_t removeNode(const str & name,const context * ctx);
		my_err_t hasNode(const str & name,const context * ctx,bucketIndex_t * id=nullptr);
		
		bool setTimes(const timeHolder tv[2]);

		bool readlnk(char * buffer, my_size_t bufferSize);

		bool isFullDir();
		
		//void loadStat(myStat * stbuf);

		bool swapContent(const str & newContent);
		
		void loadStat(fileType * T,my_mode_t * M,my_off_t * S,my_gid_t * G,my_uid_t * U,timeHolder * at,timeHolder * mt,timeHolder * ct, my_ino_t * in);
		void truncate(my_off_t newSize);
		my_off_t read(unsigned char * buf,my_size_t size, const my_off_t offset);
		my_off_t write(const unsigned char * buf,my_size_t size,const my_off_t offset);
		bool rest(void);
		bool validate_access(const context * ctx,access da,access dda=access::NONE,bool checkStickyOwner=false);
		
		void open(void); //Open a handle to the file.
		bool close(void); //Close a handle to the file.
		
		static void setFileDefaults(std::shared_ptr<chunk> meta,const context * ctx=nullptr) ;
		static void setDirMode(std::shared_ptr<chunk> meta,my_mode_t mode) ;
		static void setFileMode(std::shared_ptr<chunk> meta,my_mode_t mode, my_dev_t dev) ;
		
		
	};
	
	
};

#endif

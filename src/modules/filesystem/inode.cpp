// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#define FILESYSTEM_INODE_CPP
#include "inode.h"
#include "modules/util/endian.h"
using namespace filesystem;


void filesystem::inode_header_only::assertTypeValid() {
	
}
	
void filesystem::inode::assertTypeValid() {
	if(header.type==inode_type::NONE) {
		//We are building a new inode;
		header.type = mytype;
		header.version = latestversion;
		_ASSERT(nlinks.is_lock_free());
		_ASSERT(mode.is_lock_free());
		_ASSERT(size.is_lock_free());
		_ASSERT(uid.is_lock_free());
		_ASSERT(gid.is_lock_free());
		
	}
	_ASSERT(header.type==mytype);
}

void filesystem::inode_ctd::assertTypeValid() {
	if(header.type==inode_type::NONE) {
		header.type = mytype;
		header.version = latestversion;
	}
	/*if(mytype!=header.type) {
		CLOG((uint32_t)header.type ,"!=",(uint32_t)mytype);
	}*/
	_ASSERT(header.type==mytype);
}




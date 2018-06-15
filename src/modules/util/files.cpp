// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#include "files.h"
#include <sys/stat.h>
#include <fstream>

#ifdef _WIN32
#include <direct.h>
#endif

using namespace util;

bool util::MKDIR(const str & path) {
#ifndef _WIN32
	return mkdir(path.c_str(),0777) == 0;
#else 
	return _mkdir(path.c_str()) == 0;
#endif
	

}

bool util::fileExists(const str & name) {
	struct stat buffer;
	if(stat(name.c_str(),&buffer) == 0) {
		return true;
	}
	return false;
}

str util::getSystemString(const str & fname,uint64_t offset) {
	std::ifstream F;
	F.open(fname.c_str(),std::ios::binary|std::ios::in);
	if(F.is_open()) {
		if(offset) {
			F.seekg(offset);
		}
		return str(std::istreambuf_iterator<char>(F),std::istreambuf_iterator<char>());
	}
	return "";
}

void util::putSystemString(const str & fname, const str & content,uint64_t offset) {
	std::ofstream F;
	F.open(fname.c_str(),std::ios::binary|std::ios::out);
	if(F.is_open()) {
		if(offset) {
			F.seekp(offset);
		}
		F.write(content.data(),content.size());
	} else {
		CLOG("Failed to write to file ",fname);
	}
}

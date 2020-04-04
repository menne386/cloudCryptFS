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

bool util::fileExists(const str & name,size_t * size) {
	struct stat buffer;
	if(stat(name.c_str(),&buffer) == 0) {
		if(size) {
			*size = buffer.st_size;
		}
		return true;
	}
	return false;
}

template<class T>
void openfile(T& F, const str& fname, int mod) {
#ifdef _WIN32
	F.open(fname.c_str(), mod, _SH_DENYRW);
#else
	F.open(fname.c_str(), mod);
#endif
}

str util::getSystemString(const str & fname,uint64_t offset) {
	std::ifstream F;
	openfile(F,fname,std::ios::binary|std::ios::in);
	if(F.is_open()) {
		if(offset) {
			F.seekg(offset);
		}
		return str(std::istreambuf_iterator<char>(F),std::istreambuf_iterator<char>());
	}
	return "";
}

void util::putSystemString(const str & fname, const str & content) {
	std::ofstream F;
	openfile(F, fname, std::ios::binary|std::ios::out);
	if(F.is_open()) {
		F.write(content.data(),content.size());
	} else {
		CLOG("Failed to write to file ",fname);
	}
}
void util::replaceIntoSystemString(const str & path,const str & content,uint64_t offset) {
	std::fstream F;
	openfile(F, path, std::ios::binary|std::ios::out|std::ios::in);
	if(F.is_open()) {
		if(offset) {
			F.seekp(offset);
			_ASSERT((uint64_t)F.tellp()==offset);
		}
		F.write(content.data(),content.size());
	} else {
		CLOG("Failed to replace into file ",path);
	}
}

// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#include "files.h"
#include <fstream>
#include <filesystem>

#ifdef _WIN32
#include <direct.h>
#endif

using namespace util;

bool util::MKDIR(const str & path) {
	return std::filesystem::create_directories(path.c_str()); 
}

bool util::fileExists(const str & name,size_t * size) {
	auto stat = std::filesystem::status(name.c_str());	
	if(std::filesystem::exists(stat)) {
		if(size) {
			*size = std::filesystem::file_size(name.c_str());
		}
		return true;
	}
	return false;
}

template<class T,typename T2>
bool openfile(T& F, const str& fname, T2 mod) {
#ifdef _WIN32
	F.open(fname.c_str(), mod, _SH_DENYRW);
#else
	F.open(fname.c_str(), mod);
#endif
	return F.is_open();
}

str util::getSystemString(const str & fname,uint64_t offset) {
	std::ifstream F;
	if(openfile(F,fname,std::ios::binary|std::ios::in)) {
		if(offset) {
			F.seekg(offset);
		}
		return str(std::istreambuf_iterator<char>(F),std::istreambuf_iterator<char>());
	}
	return "";
}

bool util::putSystemString(const str & fname, const str & content) {
	std::ofstream F;
	if(openfile(F, fname, std::ios::binary|std::ios::out)) {
		F.write(content.data(),content.size());
		return true;
	} 
	CLOG("Failed to write to file ",fname);
	return false;
}
bool util::replaceIntoSystemString(const str & path,const str & content,uint64_t offset) {
	std::fstream F;
	if(openfile(F, path, std::ios::binary|std::ios::out|std::ios::in)) {
		if(offset) {
			F.seekp(offset);
			_ASSERT((uint64_t)F.tellp()==offset);
		}
		F.write(content.data(),content.size());
		return true;
	} 
	CLOG("Failed to replace into file ",path);
	return false;
}

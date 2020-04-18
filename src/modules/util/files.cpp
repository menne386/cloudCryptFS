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
	const str fnametemp = fname+"~";
	try{
		if(openfile(F, fnametemp, std::ios::binary|std::ios::out)) {
			F.write(content.data(),content.size());
			F.close();
			std::filesystem::rename(fnametemp,fname);
			return true;
		} else {
			CLOG("Failed to open file ",fnametemp, " for writing" );
		}
	} catch(std::exception & e) {
		CLOG("Failed to write file ",fname, " error: ",e.what());
	}
	return false;
}

// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef _MAIN_H
#define _MAIN_H
#include "types.h"


namespace std{
	str to_string(const char * in);
	str to_string(const str & in);
};

/*BUILDSTRING: return a string build from all the arguments. for example: BUILDSTRING(1," some text",2.14)*/
str BUILDSTRING();
template<typename T, typename... Targs> str BUILDSTRING(T value, Targs... Fargs) {
	return str(std::to_string(value).c_str()) + BUILDSTRING(Fargs...);
}

/*CLOG: Print all arguments to console log*/
void CLOG(const str & MESSAGE);
void CLOG(const char* MESSAGE);
template <typename ...Args> void CLOG(const Args& ...args) {
	CLOG(BUILDSTRING(args...));
}


template <typename T> void zeroOut(T & a) {
	for(auto & i:a) {i=0;}
}

timeHolder currentTime();

void assertfail(int condition,const char * string);
#undef _ASSERT
#define _ASSERT(CONDITION) assertfail((int)(CONDITION),"_ASSERT(" #CONDITION "); is false in " __FILE__ )

struct myfs_config {
	char *source;
	char *keyfile;
	char *keybase64;
	char *password;
	char *create;
	char *migrate;
	char *loglevel;
};

#ifdef _WIN32
int getuid();
int getgid();
#endif

#endif


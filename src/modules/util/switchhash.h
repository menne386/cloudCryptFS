// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef _SWITCHHASH_H
#define _SWITCHHASH_H
#include "main.h"
namespace swhsh {
 
	typedef unsigned int hash_t;
	 
	constexpr hash_t prime = 16777619;
	constexpr hash_t basis = 2166136261;
	 
	constexpr hash_t hash_compile_time(char const* str, hash_t last_value = basis) {
		return *str ? hash_compile_time(str+1, (*str ^ last_value) * prime) : last_value;
	}
	 
	inline hash_t hsh(const str & istr) {
		hash_t ret{basis};
	 
		for(const char & i:istr) {
			ret ^= i;
			ret *= prime;
		}
	 
		return ret;
	}
 
}


constexpr swhsh::hash_t operator "" _hsh(char const* p, my_size_t) {
	return swhsh::hash_compile_time(p);
}

#endif

// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef UTIL_ENDIAN_H_INCLUDED
#define UTIL_ENDIAN_H_INCLUDED
#include "types.h"

#ifdef _WIN32

#define __BYTE_ORDER 1
#define __LITTLE_ENDIAN 1
#define __BIG_ENDIAN 2
//#define __BYTE_ORDER    BYTE_ORDER
//#define __BIG_ENDIAN    BIG_ENDIAN
//#define __LITTLE_ENDIAN LITTLE_ENDIAN
//#define __PDP_ENDIAN    PDP_ENDIAN
#else
#include <endian.h>
#endif

#include <algorithm>

namespace util{

	
	constexpr bool isLittleEndian() {	return  __BYTE_ORDER == __LITTLE_ENDIAN; }
	constexpr bool isBigEndian() {	return  __BYTE_ORDER == __BIG_ENDIAN; }
	
	static_assert(isLittleEndian()!=isBigEndian(),"Somehow this system reports being both little and big endian");
	
	static_assert(isLittleEndian(),"Currently this program only supports little-endian systems.");
	
	
	template<typename T>
	T swap_bytes(const T input) {
		union _converter{
			T inp;
			uint8_t bytes[sizeof(T)];
		};
		_converter ret = {input};
		std::reverse(std::begin(ret.bytes),std::end(ret.bytes));
		return ret.inp;	
	}

	template<typename T>
	T host_to_little_endian(const T input) {
		if(isLittleEndian()) {
			return input;
		} 
		return swap_bytes(input);
	}
	
	template<typename T>
	T host_to_big_endian(const T input) {
		if(isBigEndian()) {
			return input;
		} 
		return swap_bytes(input);
	}
	
	template<typename T>
	T little_endian_to_host(const T input) {
		if(isLittleEndian()) {
			return input;
		} 
		return swap_bytes(input);
	}
	
	template<typename T>
	T big_endian_to_host(const T input) {
		if(isBigEndian()) {
			return input;
		} 
		return swap_bytes(input);
	}
	
	void testEndianFunction(void);
};


#endif // UTIL_ENDIAN_H_INCLUDED

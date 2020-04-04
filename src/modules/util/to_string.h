// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef __UTIL_TO_STRING_H
#define __UTIL_TO_STRING_H

#include <type_traits>
#include <cstddef>


namespace util{
	
	str to_string(const char * in);
	str to_string(const str & in);
	
	
	template<class T, std::enable_if_t<std::is_same<decltype(std::declval<const T&>().toString()), str>::value, int> = 0 >
	str to_string(const T& t) {
		return t.toString();
	}

	template<class T, std::enable_if_t<std::is_integral<T>::value, int> = 0 >
	str to_string(const T& t) {
		return std::to_string(t).c_str();
	}

	template<class T, std::enable_if_t<std::is_floating_point<T>::value, int> = 0 >
	str to_string(const T& t) {
		return std::to_string(t).c_str();
	}


	
	
};


#endif

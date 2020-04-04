// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef __UTIL_TO_STRING_H
#define __UTIL_TO_STRING_H

#include <type_traits>
#include <cstddef>




/*
template<class T>
typename std::enable_if<std::is_same<decltype(std::declval<const T&>().toString()), std::string>::value, std::string>::type my_to_string(const T &t) {
	return t.toString();
}

template<class T>
typename std::enable_if<std::is_same<decltype(std::to_string(std::declval<T&>())), std::string>::value, std::string>::type my_to_string(const T &t) {
	return std::to_string(t);
}

*/

namespace util{
	
	str to_string(const char * in);
	str to_string(const str & in);
	
	
	template<class T>
	typename std::enable_if<std::is_same<decltype(std::declval<const T&>().toString()), str>::value, str>::type to_string(const T& t) {
		return t.toString();
	}

	template<class T>
	typename std::enable_if<std::is_same<decltype(std::to_string(std::declval<T&>())), std::string>::value, str>::type to_string(const T& t) {
		return std::to_string(t).c_str();
	}


	
	
};


#endif

// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef __UTIL_TO_STRING_H
#define __UTIL_TO_STRING_H

#include <experimental/type_traits>
#include <cstddef>



template<typename T> 
using std_to_string_expression = decltype(std::to_string(std::declval<T>()));
 
template<typename T>
constexpr bool has_std_to_string = std::experimental::is_detected<std_to_string_expression, T>::value;

template<typename T>
using to_string_expression = decltype(to_string(std::declval<T>()));

template<typename T>
constexpr bool has_to_string = std::experimental::is_detected<to_string_expression, T>::value;

namespace util{
	
	str to_string(const char * in);
	str to_string(const str & in);
	
	
	template<typename T, typename std::enable_if<has_to_string<T>, int>::type = 0>
	str to_string(T const& t)
	{
	    return to_string(t);
	}

	template<typename T, typename std::enable_if<!has_to_string<T> && has_std_to_string<T>, int>::type = 0>
	str to_string(T const& t)
	{
	    return std::to_string(t).c_str();
	}
	
	
};


#endif

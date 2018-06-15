// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef __STR_H
#define __STR_H

#include "types.h"

#include <vector>

typedef std::vector<str> str_list;
size_t strSplit(const str & string, const str & delimiters, std::vector<str> & COUT);
str strtolower(const str & input) ;
str strtoupper(const str & input) ;
str& str_replace(const str &search, const str &replace, str &subject);
str  quotestring(const str & i) ;

template<typename T>
str listToString(const T & in) {
	str n;
	for(const auto & i: in) {
		n += std::to_string(i);
		n.push_back(',');
	}
	if(n.empty()==false) { 
		n.pop_back();
	}
	
	return n;
}


#endif

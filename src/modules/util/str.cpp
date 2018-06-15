// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#include "str.h"
#include <string.h>
#include <algorithm>

str strtolower(const str & input) {
	str myResult(input);
	std::transform(input.begin(), input.end(),myResult.begin(), ::tolower);
	return myResult;
}
str strtoupper(const str & input) {
	str myResult(input);
	std::transform(input.begin(), input.end(),myResult.begin(), ::toupper);
	return myResult;
}

size_t strSplit(const str & string, const str & delimiters, std::vector<str> & COUT) { 
	//Mprintf("split %s by %s\n",instring.c_str(),splitchars);
   str::size_type pos, lastPos = 0;

   using value_type = typename std::vector<str>::value_type;
   using size_type  = typename std::vector<str>::size_type;

   while(true)
   {
      pos = string.find_first_of(delimiters, lastPos);
      if(pos == str::npos)
      {
         pos = string.length();

         if(pos != lastPos )
            COUT.push_back(value_type(string.data()+lastPos,
                  (size_type)pos-lastPos ));

         break;
      }
      else
      {
         if(pos != lastPos )
            COUT.push_back(value_type(string.data()+lastPos,
                  (size_type)pos-lastPos ));
      }

      lastPos = pos + 1;
   }
   return COUT.size();
}

str  quotestring(const str & i) {
	if(i.find("\\\"")!=str::npos) {
		//CLOG("I Gets here! 1");
		return "\""+i+"\"";
	}
	str out="\"";
	out.reserve(i.length()+2);
	
	for(const char & c:i) {
		switch(c) {
			case '\"':	out+="\\\"";	break;
			case '\\':	out+="\\\\";	break;
			case '\b':	out+="\\b";	break;
			case '\r':	out+="\\r";	break;
			case '\f':	out+="\\f";	break;
			case '\n':	out+="\\n";	break;
			case '\t':	out+="\\t";	break;
			default  :	out.push_back(c);break;
		}
	}
	//CLOG("I Gets here!");
	out.push_back('"');
	return out;
}

str& str_replace(const str &search, const str &replace, str &subject) {
	str buffer;
	size_t sealeng = search.length();
	size_t strleng = subject.length();
	if (sealeng==0) {
		return subject;//no change
	}
	for(size_t i=0, j=0; i<strleng; j=0 ) {
		while (i+j<strleng && j<sealeng && subject[i+j]==search[j]) {
			j++;
		}
		if (j==sealeng) {//found 'search'
			buffer.append(replace);
			i+=sealeng;
		} else {
			buffer.append( &subject[i++], 1);
		}
	}
	subject = buffer;
	return subject;
}




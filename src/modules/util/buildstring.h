// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef __UTIL_BUILDSTRING_H
#define __UTIL_BUILDSTRING_H

#include "types.h"

#include "modules/util/to_string.h"

/*BUILDSTRING: return a string build from all the arguments. for example: BUILDSTRING(1," some text",2.14)*/
str BUILDSTRING();

template<typename T, typename... Targs> str BUILDSTRING(T value, Targs... Fargs) {
	return util::to_string(value) + BUILDSTRING(Fargs...);
}





#endif

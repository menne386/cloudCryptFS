// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "types.h"
#include "to_string.h"

namespace util{
	str to_string(const char * in) {
		return str(in);
	}
	str to_string(const str & in) {
		return str(in);
	}
}
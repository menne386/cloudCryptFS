// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef UTIL_FILES_H
#define UTIL_FILES_H
#include "main.h"

namespace util {

	/**
	 * @todo write docs
	 */
	bool MKDIR(const str & path);
	bool fileExists(const str & name, size_t * size=nullptr);
	str getSystemString(const str & path, uint64_t offset = 0);
	bool putSystemString(const str & path,const str & content);

}

#endif // UTIL_FILES_H

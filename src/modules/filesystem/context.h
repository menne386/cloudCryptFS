// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef __FILESYSTEM_CONTEXT_H
#define __FILESYSTEM_CONTEXT_H
#include "types.h"
#include <set>

namespace filesystem{

struct context{
		my_uid_t uid=0;
		my_gid_t gid=0;
		std::set<my_gid_t> gids;
		
};

};

#endif


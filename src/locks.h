// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef _GDC_LOCKS_H
#define _GDC_LOCKS_H
#include <atomic>
#include <shared_mutex>
#include <mutex>
#include <thread>
#include "modules/util/shared_recursive_mutex.h"

typedef std::recursive_mutex locktype;
typedef util::shared_recursive_mutex locktypeshared;

typedef std::lock_guard<locktype> lckguard;

typedef std::shared_lock<locktypeshared> lckshared;
typedef std::unique_lock<locktypeshared> lckunique;




#endif

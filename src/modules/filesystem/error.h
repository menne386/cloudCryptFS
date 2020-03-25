// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef FILESYSTEM_ERROR_H
#define FILESYSTEM_ERROR_H
#include "errno.h"
namespace filesystem {
	
	enum class EE :int {
		ok = 0,
		name_too_long = ENAMETOOLONG,
		invalid_syscall = ENOSYS,
		not_empty = ENOTEMPTY,
		entity_not_found = ENOENT,
		access_denied = EACCES,
		permission_denied = EPERM,
		exists = EEXIST,
		too_big = EFBIG,
		io_error = EIO,
		
	};
	
	class error{
	private:
		EE inner;
	public:
		error() : inner(EE::ok) {}
		error(EE in): inner(in) {}
		
		explicit operator bool() { return inner != EE::ok; }
		
		explicit operator int() { return (int)inner; }
		
		constexpr error & operator = (EE in) { inner = in; return *this; }
		constexpr error & operator = (const error & in) { inner = in.inner; return *this; }
		constexpr bool operator == (const error & in) { return inner == in.inner; }
		constexpr bool operator == (const EE in) { return inner == in; }
	
		error(const error & in) {
			inner = in.inner;
		}
	};
	
}

#endif // FILESYSTEM_ERROR_H


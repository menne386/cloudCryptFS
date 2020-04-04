// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef FILESYSTEM_MODE_H
#define FILESYSTEM_MODE_H

#include "types.h"

#include <atomic>
#ifndef _WIN32
#include "unistd.h"
#include <fcntl.h>
#else
#include <sys/stat.h>
//Implement the definitions that are missing on windows.
#define S_IFIFO _S_IFIFO
#ifndef S_IFLNK
#define S_IFLNK 00120000
#endif

#define S_IRUSR _S_IREAD
#define S_IWUSR _S_IWRITE
#define S_IXUSR _S_IEXEC
#define S_IRWXU (_S_IREAD|_S_IWRITE|_S_IEXEC)

#define S_ISUID 00004000
#define S_ISGID 00002000
#define S_ISVTX 00001000

#define S_IRWXG 00000070
#define S_IRGRP 00000040
#define S_IWGRP 00000020
#define S_IXGRP 00000010

#define S_IRWXO 00000007
#define S_IROTH 00000004
#define S_IWOTH 00000002
#define S_IXOTH 00000001
#endif

/*
 *proper mode conversion to allow filesystem particulars to be abstracted away.
*/

namespace filesystem {

	class mode{
		private:
		std::atomic<my_mode_t> _inner;

		public:
		
		static constexpr my_mode_t TYPE      = 00170000;
		static constexpr my_mode_t TYPE_LNK  = 00120000;
		static constexpr my_mode_t TYPE_REG  = 00100000;
		static constexpr my_mode_t TYPE_DIR  = 00040000;
		static constexpr my_mode_t TYPE_FIFO = 00010000;

		static constexpr my_mode_t FLAG_SUID = 00004000;
		static constexpr my_mode_t FLAG_SGID = 00002000;
		static constexpr my_mode_t FLAG_SVTX = 00001000;

		static constexpr my_mode_t USER_RWX  = 00000700;
		static constexpr my_mode_t USER_R    = 00000400;
		static constexpr my_mode_t USER_W    = 00000200;
		static constexpr my_mode_t USER_X    = 00000100;

		static constexpr my_mode_t GROUP_RWX = 00000070;
		static constexpr my_mode_t GROUP_R   = 00000040;
		static constexpr my_mode_t GROUP_W   = 00000020;
		static constexpr my_mode_t GROUP_X   = 00000010;

		static constexpr my_mode_t WORLD_RWX = 00000007;
		static constexpr my_mode_t WORLD_R   = 00000004;
		static constexpr my_mode_t WORLD_W   = 00000002;
		static constexpr my_mode_t WORLD_X   = 00000001;

		static constexpr bool isNativeCompatible = 
		 TYPE      == S_IFMT  && 
		 TYPE_LNK  == S_IFLNK && 
		 TYPE_REG  == S_IFREG && 
		 TYPE_DIR  == S_IFDIR && 
		 TYPE_FIFO == S_IFIFO && 

		 FLAG_SUID == S_ISUID && 
		 FLAG_SGID == S_ISGID && 
		 FLAG_SVTX == S_ISVTX && 

		 USER_RWX  == S_IRWXU && 
		 USER_R    == S_IRUSR && 
		 USER_W    == S_IWUSR && 
		 USER_X    == S_IXUSR && 

		 GROUP_RWX == S_IRWXG && 
		 GROUP_R   == S_IRGRP && 
		 GROUP_W   == S_IWGRP && 
		 GROUP_X   == S_IXGRP && 

		 WORLD_RWX == S_IRWXO && 
		 WORLD_R   == S_IROTH &&
		 WORLD_W   == S_IWOTH && 
		 WORLD_X   == S_IXOTH; 
		 		 
		 inline bool is_lock_free() { return _inner.is_lock_free(); }
		 
		 inline my_mode_t type() const { return _inner.load() & TYPE; }
		 inline void setType(my_mode_t t) { _inner |= (t&TYPE); }
		 
		 inline void set(const my_mode_t f) { _inner |= f; }
		 inline void clear(const my_mode_t f) { _inner |= f; _inner ^= f; }
		 
		 inline my_mode_t load() const { return _inner.load(); }
		 inline operator my_mode_t () const { return load(); }
		 
		 inline mode & operator = (const my_mode_t  in) { _inner.store(in); return *this; } 
		 
	};
	
	static_assert(mode::isNativeCompatible,"This system is not posix compatible enough to compile this code.");

};

#endif // FILESYSTEM_MODE_H

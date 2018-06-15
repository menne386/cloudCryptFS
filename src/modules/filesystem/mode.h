// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef FILESYSTEM_MODE_H
#define FILESYSTEM_MODE_H

#include <atomic>

/*
 *
 *
 * @todo: proper mode conversion to allow filesystem particulars to be abstracted away.
 * @todo: atomic mode!
#define S_IFMT  00170000
#define S_IFSOCK 0140000
#define S_IFLNK	 0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000

#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

#define S_ISLNK(m)	(((m) & S_IFMT) == S_IFLNK)
#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m)	(((m) & S_IFMT) == S_IFSOCK)

#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010

#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001
*/

namespace filesystem {

	class mode{
		private:
		std::atomic_int _inner;

		public:
		static constexpr int TYPE      = 00170000;
		static constexpr int TYPE_LNK  = 00120000;
		static constexpr int TYPE_REG  = 00100000;
		static constexpr int TYPE_DIR  = 00040000;
		static constexpr int TYPE_FIFO = 00010000;

		static constexpr int FLAG_SUID = 00004000;
		static constexpr int FLAG_SGID = 00002000;
		static constexpr int FLAG_SVTX = 00001000;

		static constexpr int USER_RWX  = 00000700;
		static constexpr int USER_R    = 00000400;
		static constexpr int USER_W    = 00000200;
		static constexpr int USER_X    = 00000100;

		static constexpr int GROUP_RWX = 00000070;
		static constexpr int GROUP_R   = 00000040;
		static constexpr int GROUP_W   = 00000020;
		static constexpr int GROUP_X   = 00000010;

		static constexpr int WORLD_RWX = 00000007;
		static constexpr int WORLD_R   = 00000004;
		static constexpr int WORLD_W   = 00000002;
		static constexpr int WORLD_X   = 00000001;

		/*static constexpr bool isNativeCompatible = 
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


		int toNative();
		static mode makeFromSystem(int m);*/


	};

};

#endif // FILESYSTEM_MODE_H

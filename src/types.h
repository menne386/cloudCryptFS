// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef _CCFS_TYPES_H
#define _CCFS_TYPES_H
#include <string>
#include <vector>
#include <memory>
#include <atomic>

#ifdef _WIN32
#define __EXPORT
#else
#define __EXPORT 
#endif

extern "C" void __EXPORT sodium_memzero(void * const pnt, const size_t len);
#undef max

using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;

//#include "secblock.h"
#include "modules/filesystem/error.h"



template<class _Ty>
class EraseMemAlloc {
		private:
		std::allocator<_Ty> _alloc;
public:
		typedef std::size_t size_type;		
		using value_type = _Ty;


    template<class _Other>
    EraseMemAlloc<_Ty>& operator=(const EraseMemAlloc<_Other>&) noexcept {   
    	// assign from a related EraseMemAlloc (do nothing)
        return (*this);
    }

    _Ty * allocate( size_type n ) {
        return _alloc.allocate(n);
    }
    _Ty * allocate( size_type n, const _Ty* p) {
        return _alloc.allocate(n,p);
    }

    void deallocate(_Ty* p, size_type n ) {
    	sodium_memzero(p,n);
    	//@todo: should securely erase memory here
    	_alloc.deallocate(p,n);
    }
};

template <typename T, typename U>
inline bool operator == (const EraseMemAlloc<T>&, const EraseMemAlloc<U>&) {
  return true;
	}

	template <typename T, typename U>
	inline bool operator != (const EraseMemAlloc<T>& a, const EraseMemAlloc<U>& b) {
	  return !(a == b);
		}


typedef std::basic_string<char, std::char_traits<char>, EraseMemAlloc<char>  > str;

typedef std::vector<uint8_t,EraseMemAlloc<uint8_t>> secbyteblock;

#define _STRTOBYTESIZE(NAME) reinterpret_cast<const uint8_t*>(NAME.data()),NAME.size()
#define _STRTOBYTESIZEOUT(NAME) reinterpret_cast<uint8_t*>(&NAME[0]),NAME.size()
#define _STRTOCHARPSIZE(NAME) reinterpret_cast<const char*>(NAME.data()),NAME.size()

namespace std {

  template <> struct hash<str> {
    std::size_t operator()(const str& k) const { return std::hash<std::string>()(k.c_str()); }
  };

}

struct timeHolder {
	timeHolder() {
		tv_sec = 0;
		tv_nsec = 0;
	}
	timeHolder(const timeHolder & in) {
		tv_sec = in.tv_sec.load();
		tv_nsec = in.tv_nsec.load();
	}
	timeHolder & operator=(const timeHolder & in) {
		tv_sec = in.tv_sec.load();
		tv_nsec = in.tv_nsec.load();
		return *this;
	}
	std::atomic<time_t> tv_sec;				/* Seconds.  */
	std::atomic<int64_t> tv_nsec;     /* Nanoseconds.  */
};

#ifdef _WIN32 
#define MYSTAT struct FUSE_STAT
typedef __int64 my_off_t;
typedef filesystem::error my_err_t;
typedef unsigned long long  my_size_t;
typedef unsigned int  my_gid_t;
typedef unsigned int  my_uid_t;
typedef unsigned int  my_mode_t;
typedef unsigned int  my_dev_t;
typedef uint64_t my_ino_t;

#else
#include <sys/types.h>

#define myStat struct stat
#define MYSTAT struct stat
typedef filesystem::error my_err_t;
typedef off_t my_off_t;
typedef size_t my_size_t;
typedef uid_t my_uid_t;
typedef gid_t my_gid_t;
typedef mode_t my_mode_t; 
typedef dev_t my_dev_t;
typedef ino_t my_ino_t;
#endif

namespace filesystem{
	enum class access:my_mode_t{
		NONE=0,
		X=1,
		W=2,
		WX=3,
		R=4,
		RX=5,
		RW=6,
		RWX=7,
	};
	enum class flags:my_mode_t{
		NONE=0,
		//STICKY
	};
};

namespace crypto{
	class protocolInterface;
	class block;
	class key;
}


#endif

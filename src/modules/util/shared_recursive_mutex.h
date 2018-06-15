// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef UTIL_SHARED_RECURSIVE_MUTEX_H
#define UTIL_SHARED_RECURSIVE_MUTEX_H

#include <atomic>
#include <thread>

namespace util {

	/**
	 * This is a shared, recursive mutex based on atomic spinlocks.
	 * 
	 * The lock ASSERTS that no unique lock is owned by the thread when you lock_shared
	 * If the thread already owns a shared lock, and then tries to unique_lock, the lock will spin forever! 
	 * So take care when using the shared locks!!!
	 * 
	 */
	class shared_recursive_mutex final{
	private:
		std::atomic_flag lck = ATOMIC_FLAG_INIT;
		std::thread::id owner;
		std::atomic_int count;
		std::atomic_int shared_locks;
	public:
		shared_recursive_mutex();
		
		void lock();
		bool try_lock();
		void unlock();
		
		void unique_lock() { lock(); };
		bool try_unique_lock() { return try_lock(); };
		void unique_unlock() { unlock(); };
		
		void lock_shared();
		bool try_lock_shared();
		void unlock_shared();
		
		bool hasUniqueLock();
	};

}

#endif // UTIL_SHARED_RECURSIVE_MUTEX_H

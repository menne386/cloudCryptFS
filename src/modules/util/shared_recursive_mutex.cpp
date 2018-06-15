// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#include "shared_recursive_mutex.h"
#include "main.h"

using namespace util;

shared_recursive_mutex::shared_recursive_mutex() {
	count.store(0);
	shared_locks.store(0);
	owner = std::thread::id();
}

void shared_recursive_mutex::lock() {
	auto this_id = std::this_thread::get_id();
	if(owner == this_id) {
		// recursive locking
		count++;
	}	else {
		while (lck.test_and_set(std::memory_order_acquire)) { // acquire lock
			std::this_thread::yield();
		}
		while(shared_locks.load()>0) {
			std::this_thread::yield();
		}
		owner = this_id;
		count.store(1);
	}
}
bool shared_recursive_mutex::try_lock() {
	auto this_id = std::this_thread::get_id();
	if(owner == this_id) {
		// recursive locking
		count++;
		return true;
	}	else {
		if(!lck.test_and_set(std::memory_order_acquire)) {
			if(shared_locks.load()>0) {
				lck.clear(std::memory_order_release);
				return false;
			}
			owner = this_id;
			count.store(1);
			return true;
		}
		return false;
	}
}
void shared_recursive_mutex::unlock() {
	if(count > 1) {
		// recursive unlocking
		count--;
	}	else {
		// normal unlocking
		owner = std::thread::id();
		count.store(0);
		lck.clear(std::memory_order_release);
	}
}
		
void shared_recursive_mutex::lock_shared() {
	auto this_id = std::this_thread::get_id();
	_ASSERT(owner != this_id);
	
	while (lck.test_and_set(std::memory_order_acquire)) { // acquire lock
		std::this_thread::yield();
	}
	
	shared_locks++;
	
	//CLOG(std::hash<std::thread::id>()(std::this_thread::get_id())," acquire ",(uint64_t)this," cnt: ",shared_locks.load());
	lck.clear(std::memory_order_release);
}
		
bool shared_recursive_mutex::try_lock_shared() {
	if(!lck.test_and_set(std::memory_order_acquire)) {
		shared_locks++;
		//CLOG(std::hash<std::thread::id>()(std::this_thread::get_id())," acquire(try) ",(uint64_t)this," cnt: ",shared_locks.load());
		lck.clear(std::memory_order_release);
		return true;
	}
	return false;
}
		
void shared_recursive_mutex::unlock_shared() {
	//CLOG(std::hash<std::thread::id>()(std::this_thread::get_id())," release ",(uint64_t)this," cnt: ",shared_locks.load());
	auto t = shared_locks--;
	_ASSERT(t>-1);
}

bool shared_recursive_mutex::hasUniqueLock() {
	auto this_id = std::this_thread::get_id();
	if(owner == this_id) {
		return true;
	}
	return false;
}

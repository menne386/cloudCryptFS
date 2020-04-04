// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef UTIL_SHARED_ATOMIC_SHARED_PTR_H
#define UTIL_SHARED_ATOMIC_SHARED_PTR_H

#include <memory>
#include <atomic>

#ifndef __cplusplus
  #error C++ is required
#elif __cplusplus < 201703L
  #error C++17 is required
#endif

namespace util{
#if __cplusplus > 202003L
	template <typename T> 
	using atomic_shared_ptr = std::atomic<std::shared_ptr<T>>;
#else
	/*The following atomic_shared_ptr class is a C++20 compatible wrapper around the pre C++20 api. (only operations that are used are implemented) */

	template <typename T> 
	class atomic_shared_ptr{
		private:
		std::shared_ptr<T> _inner;
		public:
		
		constexpr atomic_shared_ptr() noexcept = default;
		atomic_shared_ptr(std::shared_ptr<T> desired) noexcept : _inner(desired) {}
		atomic_shared_ptr(const atomic_shared_ptr&) = delete;
		
		void operator=(const atomic_shared_ptr&) = delete;
		
		void operator=(std::shared_ptr<T> desired) noexcept {
			store(desired);
		}
		
		void store(std::shared_ptr<T> desired,std::memory_order order = std::memory_order_seq_cst) noexcept {
			std::atomic_store_explicit(&_inner,desired,order);
		}
		
		std::shared_ptr<T> load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
			return std::atomic_load_explicit(&_inner,order);
		}
		
		operator std::shared_ptr<T> () const noexcept {
			return load();
		}
		
		std::shared_ptr<T> exchange(std::shared_ptr<T> desired,std::memory_order order = std::memory_order_seq_cst) noexcept {
			return atomic_exchange_explicit(&_inner,desired,order);
		}
			
		bool compare_exchange_strong(std::shared_ptr<T>& expected, std::shared_ptr<T> desired) noexcept {
			return atomic_compare_exchange_strong(&_inner,&expected,desired);
		}
	};

#endif

}//namespace util

#endif

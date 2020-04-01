// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
/**
 * Implements a basic map with locking.
 **/

#ifndef UTIL_PROTECTED_UNORDERED_MAP_H
#define UTIL_PROTECTED_UNORDERED_MAP_H

#include <unordered_map>
#include <shared_mutex>
#include <atomic>

namespace util {
	template<typename K,typename T,typename MUTEX=std::shared_mutex>
	class protected_unordered_map final {
	private:
		std::unordered_map<K, T> _map;
		MUTEX _mut;
		std::atomic<size_t> _size;
	public:
		protected_unordered_map() {
			_size = 0;
			_map.clear();
		}
		protected_unordered_map(const protected_unordered_map&) = delete;


		T get(const K& key) {
			std::shared_lock<MUTEX> l(_mut);
			auto it = _map.find(key);
			if (it != _map.end()) {
				return it->second;
			}
			return T();
		}

		void insert(const K& key, T value) {
			std::unique_lock<MUTEX> l(_mut);
			_map[key] = value;
			_size = _map.size();
		}

		size_t erase(const K& key) {
			std::unique_lock<MUTEX> l(_mut);
			auto ret = _map.erase(key);
			_size = _map.size();
			return ret;
		}

		void clear() {
			std::unique_lock<MUTEX> l(_mut);
			_map.clear();
			_size = 0;
		}

		size_t size() { return _size.load(); }

		std::vector<T> list() {
			std::shared_lock<MUTEX> l(_mut);
			std::vector<T> ret;
			for (auto& i : _map) {
				ret.push_back(i.second);
			}
			return ret;
		}
		
		std::unordered_map<K,T> clone() {
			std::shared_lock<MUTEX> l(_mut);
			std::unordered_map<K,T> ret = _map;
			return ret;
		}

	};
};

#endif

// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
/**
 * This templated class can maintain a list of shared_ptr's.
 * reading and writing operations to the list are atomic. (+shared lock)
 * The swap and truncate operation lock.
 */
#ifndef UTIL_SHARED_ATOMIC_SHARED_PTR_LIST_H
#define UTIL_SHARED_ATOMIC_SHARED_PTR_LIST_H

#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <algorithm>

namespace util{
	
template<typename T,typename MUTEX=std::shared_mutex>
class atomic_shared_ptr_list{ 
public:
	typedef std::vector<std::shared_ptr<T>> listType;
	typedef uint64_t size_t;
	typedef std::shared_lock<MUTEX> lckshared;
	typedef std::unique_lock<MUTEX> lckunique;
	atomic_shared_ptr_list() {
		_size.store(0);
	}
	
	size_t getSize() {
		return _size.load();
	}
	
	listType getRange(size_t fromItem, size_t numItems) {
		lckshared l(_mut);
		listType ret;
		if(fromItem+numItems <=_size) {
			ret.resize(numItems,nullptr);
			size_t offset = fromItem;
			for(auto & i: ret) {
				i = std::atomic_load(&_list[offset]);
				++offset;
			}
		}
		return ret;	
	}
	
	listType getAll() {
		lckshared l(_mut);
		listType ret;
		ret.resize(_size.load(),nullptr);
		size_t offset = 0;
		for(auto & i: ret) {
			i = std::atomic_load(&_list[offset]);
			++offset;
		}
		return ret;	
	}
	
	
	bool updateRange(size_t fromItem, listType & newItems) {
		lckshared l(_mut);
		if(fromItem+newItems.size()<=_size) {
			size_t offset = fromItem;
			for(auto & i: newItems) {
				std::atomic_store(&_list[offset],i);
				++offset;
			}
			return true;
		}
		return false;
	}
	
	listType truncateAndReturn(size_t newSize,std::shared_ptr<T> newItem) {
		listType ret;
		lckunique l(_mut);
		if(_size>newSize) {
			for(size_t a=newSize;a<_size;++a) {
				ret.push_back(_list[a]);
			}
		} else if(_size<newSize) {
			ret.resize(newSize-_size,nullptr);
		}
		_list.resize(newSize,newItem);
		_size = newSize;
		return ret;
	}
	
	void swap(listType & list) {
		lckunique l(_mut);
		std::swap(_list,list);
		_size = _list.size();
	}

private:
	
	listType _list;
	MUTEX _mut;
	std::atomic<size_t> _size;
};

};//Namespace util


#endif

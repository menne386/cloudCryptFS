// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef FILESYSTEM_CHUNK_H
#define FILESYSTEM_CHUNK_H
#include "main.h"
#include "hash.h"
#include <array>
namespace filesystem {
	const unsigned chunkSize = 4096;
	const unsigned chunksInBucket = 256;

	class file;
	class bucket;
	/**
	 * @todo write docs
	 */
	class chunk {
	private:
		friend class bucket;
		std::array<unsigned char,chunkSize> data;
	public:
		chunk();
		~chunk();
		crypto::sha256sum getHash();
		void read(my_off_t offset,my_size_t size,unsigned char * output) const;
		std::shared_ptr<chunk> write(my_off_t offset,my_size_t size,const unsigned char * input) const;
		std::shared_ptr<chunk> clone() const;
		static std::shared_ptr<chunk> newChunk(my_size_t size, const unsigned char * input);
		
		bool compareChunk(shared_ptr<chunk> c);
		
		template<typename T> T* as() { 
			static_assert(sizeof(T) == chunkSize ,"type should be of chunkSize"); 
			auto * ptr = reinterpret_cast<T*>(&data[0]);
			ptr->assertTypeValid();
			return ptr;
		}

	};

}

#endif // FILESYSTEM_CHUNK_H

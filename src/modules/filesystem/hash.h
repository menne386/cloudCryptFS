// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef FILESYSTEM_HASH_H
#define FILESYSTEM_HASH_H
#include "modules/script/script_complex.h"
#include "modules/crypto/sha256.h"
#include "types.h"
#include <atomic>
#include <unordered_map>

namespace filesystem {
	class chunk;
	/**
	 * @todo write docs
	 * new design ideas: - use an atomic for the refCount,
	 */
	class bucketIndex_t {
		public:
			union{
				struct {
					uint64_t index : 8;
					uint64_t bucket : 56;
				};
				uint64_t fullindex;
			};
		operator uint64_t() const { return fullindex; }
		bool operator==(const bucketIndex_t& i) const {	return i.fullindex == fullindex; }
	};
	

	static_assert(sizeof(bucketIndex_t)==sizeof(uint64_t),"buckerIndex_t wrong size");

	class hash {
	private:
		typedef uint32_t flagtype;
		crypto::sha256sum _hsh;
		bucketIndex_t bucketIndex;
		std::atomic<script::int_t> refcnt;		
		std::shared_ptr<chunk> data;
		std::atomic<flagtype> flags;
		void clearFlags(flagtype in);
		void setFlags(flagtype in);
		bool isFlags(flagtype in);
	public:
		static constexpr unsigned REFCNT = 0;
		static constexpr unsigned BUCKETID = 1;
		static constexpr unsigned BUCKETINDEX = 2;
		static constexpr flagtype FLAG_DELETED     = 1 << 0;
		static constexpr flagtype FLAG_NOAUTOSTORE = 1 << 1;
		static constexpr flagtype FLAG_NOAUTOLOAD  = 1 << 2;

		const crypto::sha256sum & getHashPrimitive() {return _hsh;};
		script::str_t getHashStr() const { return _hsh.toShortStr(); };
		const bucketIndex_t getBucketIndex() const { return bucketIndex; };
		script::int_t getRefCnt();
		script::int_t incRefCnt();
		script::int_t decRefCnt();
		
		bool compareChunk(shared_ptr<chunk> c);
		
		bool hasData() { return data!=nullptr; }
		
		
		hash(const crypto::sha256sum & ihash, const bucketIndex_t ibucket, const script::int_t irefcnt ,std::shared_ptr<chunk> idata, flagtype iflags = 0);
		~hash();
		
		

		void read(my_off_t offset,my_size_t size,unsigned char * output);
		std::shared_ptr<hash> write(my_off_t offset,my_size_t size,const unsigned char * input);
		bool rest(void);
		
		//hash(const str & ihash, const script::int_t ibucket, const script::int_t iindex, const script::int_t irefcnt, std::shared_ptr<chunk> idata);

	};
	typedef std::shared_ptr<hash> hashPtr;

}

template<> struct std::hash<const ::filesystem::bucketIndex_t> {
	std::size_t operator()(const ::filesystem::bucketIndex_t& s) const noexcept {
		return s.fullindex; 
	}
};


#endif // FILESYSTEM_HASH_H

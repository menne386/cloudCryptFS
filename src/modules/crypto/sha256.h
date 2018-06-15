// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef CRYPTO_SHA256_H
#define CRYPTO_SHA256_H
#include "types.h"

namespace crypto {


	class sha256sum{
		friend struct std::hash<const crypto::sha256sum>;
	private:
		union{
			uint64_t data[4];
			char _characters[32];
			unsigned char _bytes[32];
		};
		
	public:
		
		//sha256sum(const sha256sum& in);
		//sha256sum(const str & data);
		sha256sum(const unsigned char * in,const size_t size);
		~sha256sum();
		void copy(unsigned char * out);
		str toLongStr() const;
		str toShortStr() const;
		bool operator < (const sha256sum & in) const;
		bool operator == (const sha256sum & in) const;
		
	};
	
	static_assert(sizeof(sha256sum) == 32," sizeof(sha256sum) needs to be 32");
}

template<> struct std::hash<const crypto::sha256sum> {
	std::size_t operator()(const crypto::sha256sum & s) const noexcept {
		//https://stackoverflow.com/questions/1646807/quick-and-simple-hash-code-combinations/1646913#1646913
		std::size_t h = 17;
		for(int a=0;a<4;a++) {
			h = h * 31 + s.data[a]; 
		}
		return h;		
	}
};


#endif // CRYPTO_SHA256_H

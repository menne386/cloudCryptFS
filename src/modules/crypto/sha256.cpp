// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#include "sha256.h"
#include "main.h"
#include <sodium.h>

namespace crypto {

	sha256sum::sha256sum(const unsigned char * in,const size_t size) {
		_ASSERT(crypto_hash_sha256_BYTES == sizeof(_characters));
		crypto_hash_sha256(_bytes, in, size);
	}
	
	sha256sum::~sha256sum() {
		sodium_memzero(_characters,sizeof(_characters));
	}
	
	str sha256sum::toLongStr() const {
		str encoded;
		encoded.resize((sizeof(_characters)*2)+1);
		sodium_bin2hex(&encoded[0], encoded.size(),_bytes,sizeof(_bytes));
		encoded.resize(sizeof(_characters)*2);
		return encoded;
	}
	
	str sha256sum::toShortStr() const {
		str encoded;
		encoded.resize(sodium_base64_encoded_len(sizeof(_characters),sodium_base64_VARIANT_ORIGINAL));
		sodium_bin2base64(&encoded[0], encoded.size(),_bytes,sizeof(_bytes),sodium_base64_VARIANT_ORIGINAL);
		return encoded;
	}
	
	bool crypto::sha256sum::operator<(const crypto::sha256sum& in) const {
		for(auto a=0;a<4;a++) {
			if(in.data[a]<data[a]) {
				return true;
			}
		}
		return false;
	}
	
	bool sha256sum::operator==(const crypto::sha256sum& in) const {
		for(auto a=0;a<4;a++) {
			if(in.data[a]!=data[a]) {
				return false;
			}
		}
		return true;
	}

	void sha256sum::copy(unsigned char* out) {
		std::copy(_bytes,_bytes+sizeof(_bytes),out);
	}

	
	

};

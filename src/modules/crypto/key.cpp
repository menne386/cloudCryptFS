// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#include "key.h"
#include "main.h"
#include "sha256.h"

#include <sodium.h>




using namespace crypto;



void xorbuf(uint8_t * buf, const uint8_t * msk,const my_size_t size) {
	if(size % sizeof(unsigned) ==0) {
		auto b = reinterpret_cast<unsigned *>(buf);
		auto m = reinterpret_cast<const unsigned *>(msk);
		for(my_size_t a = 0;a<size/sizeof(unsigned);++a) {
			b[a] ^= m[a];
		}	
	} else {
		for(my_size_t a = 0;a<size;++a) {
			buf[a] ^= msk[a];
		}
	}
}

/*
done: keys should be protected better. We could XOR the key with a random key.. reducing the number of exposed keys in memory to just 1...
@todo: perhaps even use SGX to protect that specific key?
*/


const uint8_t * key::getMaster(const my_size_t size) {
	
	static uint8_t _masterkeys[64] = {0};
	static_assert(sizeof(_masterkeys)==64,"_masterkeys should be 64 bytes");
	_ASSERT(size<sizeof(_masterkeys));
	if(_masterkeys[0]==0 && _masterkeys[1]==0) {
		//CLOG("Generating master key:");
		randombytes_buf(_masterkeys,sizeof(_masterkeys));
	}
	return &_masterkeys[0];
}

void key::protect(void) {
	auto * mk = getMaster(size());
	xorbuf(&_realKey[0],mk,size());	
}

key::key(const secbyteblock & _keydata) {
	_realKey = _keydata;
	protect();
}

key::key(const str & _keydata) {
	_realKey.resize(_keydata.size());
	std::copy(_keydata.begin(),_keydata.end(),_realKey.begin());
	protect();
}


secbyteblock key::use() {
	secbyteblock ret(_realKey);
	auto * mk = getMaster(size());
	xorbuf(&(ret)[0],mk,size());
	return ret;
}

my_size_t key::size() {return _realKey.size();}

void key::toBase64String(str & output) {
	auto dta = use();
	output.resize(sodium_base64_encoded_len(size(),sodium_base64_VARIANT_ORIGINAL),' ');
	auto * t = sodium_bin2base64(&output[0],output.size(),_STRTOBYTESIZE(dta),sodium_base64_VARIANT_ORIGINAL);
	while(output.back()==0) {
		output.pop_back();
	}
	_ASSERT(t!=nullptr);
}



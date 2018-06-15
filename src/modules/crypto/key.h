// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef CRYPTO_KEY_H
#define CRYPTO_KEY_H

#include "types.h"


namespace crypto{


	class block;
	

	class key {
	private:
		static const uint8_t * getMaster(const my_size_t size);
		// function that unprotects the key for usage.
		secbyteblock _realKey;
		void protect(void);
	public:

		
		key(const secbyteblock & _keydata);
		key(const str & _keydata);

		secbyteblock use();
	
		my_size_t size() ;
		void toBase64String(str & output);
		
	};
	
	
	template<typename T> void setKeyWithIV(T & d,shared_ptr<key> ikey,shared_ptr<key> iiv) {
		
		d.SetKeyWithIV( ikey->use(), ikey->size(),iiv->use(),iiv->size() );
	}
	
};

#endif

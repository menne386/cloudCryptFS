// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef CRYPTO_BLOCK_H
#define CRYPTO_BLOCK_H

#include "types.h"

namespace crypto{
	class key;
	class protocolInterface;
	
	enum class blockInput{
		CLEARTEXT_STOREIV,
		CIPHERTEXT_IV
	};

	class block {
	private:
		protocolInterface * _protocol;
		std::shared_ptr<key> _key;
		str _data;
		bool _encrypted;
	public:
		block(std::shared_ptr<key> ikey,protocolInterface * iprotocol,str & idata,blockInput itype);
		~block();

		str getCipherText(bool asBase64=false);
		str getClearText();
		

		bool setEncrypted(bool iencrypted);
		bool isEncrypted() {return _encrypted;}
		
		
	};
	
	
};

#endif

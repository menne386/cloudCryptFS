// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#include "block.h"
#include "main.h"

#include <sodium.h>

#include "key.h"
#include "protocol.h"

using namespace crypto;

block::block(std::shared_ptr<key> ikey,protocolInterface * iprotocol,str & idata,blockInput itype) {
	
	_key = ikey;
	_protocol = iprotocol;

	switch(itype) {
		case blockInput::CLEARTEXT_STOREIV:
			_encrypted = false;
			_data = std::move(idata);
			break;	
		case blockInput::CIPHERTEXT_IV:
			_encrypted = true;
			_data = std::move(idata);
			break;
	}
}

block::~block() {
}


str block::getCipherText(bool asBase64) {
	if(!isEncrypted()) {
		if(!setEncrypted(true)) {
			return "";
		}
	}
	if(asBase64) {
		str temp;
		temp.resize(sodium_base64_encoded_len(_data.size(),sodium_base64_VARIANT_ORIGINAL));
		auto * t = sodium_bin2base64(&temp[0],temp.size(),_STRTOBYTESIZE(_data),sodium_base64_VARIANT_ORIGINAL);
		_ASSERT(t!=nullptr);

		return temp;
	}

	return _data;
}

str block::getClearText() {
	if(isEncrypted()) {
		if(!setEncrypted(false)) {
			return "";
		}
	}
	return _data;
}




bool block::setEncrypted(bool iencrypted) {
	if(iencrypted != _encrypted) {
		//move the original
		str original = std::move(_data);
		if(iencrypted) {
			//_data to ciphertext
			try	{
				_protocol->encrypt(_key,original,_data);	
			} catch( std::exception const & e )	{
				//_data = std::move(original);
				CLOG("Encryption error: ",e.what());
				return false;
			}
			
		} else {
			//_data to plaintext
			try {
				_protocol->decrypt(_key,original,_data);
			} catch( std::exception const & e )	{
				_data = std::move(original);
				CLOG("Decryption error: ",e.what());
				return false;
			}
		}

		_encrypted = iencrypted;
		return true;
	}
	return true;
}

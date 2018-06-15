// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#define CRYPTO_PROTOCOL_CPP
#include "protocol.h"
#include <stdexcept>
#include <sodium.h>
#include "main.h"
#include "sha256.h"
#include "modules/util/str.h"
/**
 * protocol.cpp's job is to provide a standard for all encryption/decryption/hashing/filename hiding operations.
 * The idea is that when a protocol has a weakness, we can migrate to a new protocol version. 
 *
 * Migration function would be:
 * -Generate new config
 * -create new protocol instance
 * -Use old protocol to decrypt all chunks/meta info
 * -Use new protocol to encrypt all chunks/meta info
 */
 
using namespace crypto;

#define _UNDEFINED() throw std::logic_error(std::string("Protocol undefined for function ")+std::string( __func__) );
str protocolInterface::getVersion() { return BUILDSTRING(majorVer,".",minorVer,isAlt?"b":""); }


template<size_t V, size_t MV,bool VA>
class protocol : public protocolInterface {
	private:
	shared_ptr<key> _filenamepublic,_filenamesecret,_passwordpublic;	
	public:
	protocol(std::shared_ptr<script::ComplexType> config): protocolInterface(V,MV,VA,config) {
		if(V==1) {
			//Protocol version 1 uses crypto_aead_xchacha20poly1305_ietf construction
			keySize = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;
			tagSize = 16U;//@todo: does libsodium have a definition of this?
			IVSize = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
			_filenamepublic = loadKey(config->getOPtr("filenamepublic"));
			if(MV>=1) {
				_passwordpublic = loadKey(config->getOPtr("passwordpublic"));
			}
		} else {
			_UNDEFINED();
		}
	}
	
	void buildNewConfig() {
		if(V==1) {
			//Set options in config
			_filenamepublic = newRandomKey();
			storeKey(_filenamepublic,config->getOPtr("filenamepublic"));
			
			if(MV>=1) {
				//From 1.1 forward we need a password public
				_passwordpublic = newRandomKey();
				storeKey(_passwordpublic,config->getOPtr("passwordpublic"));
				config->getI("opslimit") = crypto_pwhash_argon2id_OPSLIMIT_MODERATE;
				config->getI("memlimit") = crypto_pwhash_argon2id_MEMLIMIT_MODERATE;
			}
		} else {
			_UNDEFINED();
		}
	}
	
	void enterPassword(const str & input) {
		//How the protocol handles entering a password:
		if(V == 1) {
			{
				
				if(MV==0) {
					secbyteblock _realKey;
					_realKey.resize(keySize,0);					
					_ASSERT(sizeof(sha256sum) == keySize);
					sha256sum s(_STRTOBYTESIZE(input));
					s.copy(&_realKey[0]);
					_protokey = make_shared<crypto::key>(_realKey);
					{
						auto hsh = _filenamepublic->use();
						str sec(_STRTOCHARPSIZE(hsh));
						sec+=":"+input;
						sha256sum s2(_STRTOBYTESIZE(sec));
						s2.copy(&_realKey[0]);
						_filenamesecret = make_shared<crypto::key>(_realKey);
					}
				} else if(MV>=1) {
					CLOG(crypto_pwhash_argon2id_SALTBYTES,":",keySize);
					//From version 1.1 & forward argon2 is used to generate the keys and the secret.
					_ASSERT(_filenamepublic!=nullptr);
					_ASSERT(_passwordpublic!=nullptr);
					_ASSERT(crypto_pwhash_argon2id_SALTBYTES <= _filenamepublic->size());
					_ASSERT(crypto_pwhash_argon2id_SALTBYTES <= _passwordpublic->size());
					{
						secbyteblock _realKey;
						_realKey.resize(keySize,0);
						auto salt = _passwordpublic->use();
						_ASSERT(crypto_pwhash_argon2id(
							_STRTOBYTESIZEOUT(_realKey),
	                       _STRTOCHARPSIZE(input),
	                       salt.data(),
	                       config->getI("opslimit"), config->getI("memlimit"),
	                       crypto_pwhash_argon2id_ALG_ARGON2ID13
						)==0);
						_protokey = make_shared<crypto::key>(_realKey);
					}
					{
						secbyteblock _realKey;
						_realKey.resize(keySize,0);						
						auto salt = _filenamepublic->use();
						_ASSERT(crypto_pwhash_argon2id(
							_STRTOBYTESIZEOUT(_realKey),
	                       _STRTOCHARPSIZE(input),
	                       salt.data(),
	                       config->getI("opslimit"), config->getI("memlimit"),
	                       crypto_pwhash_argon2id_ALG_ARGON2ID13
						)==0);
						_filenamesecret = make_shared<crypto::key>(_realKey);
					}
					
				}
			}
		} else {
			_UNDEFINED();
		}
		_ASSERT(_filenamesecret!=nullptr);
		_ASSERT(_protokey!=nullptr);
	}
	
	void regenerateEncryptionKey(void) {
		_key = newRandomKey();
	}
	
	/* Store a Key to a JSON object*/
	bool storeKey(shared_ptr<key> k,std::shared_ptr<script::ComplexType> configNode ) {
		k->toBase64String(configNode->getS("key"));
		return true;
	}
	
	shared_ptr<key> loadKey(std::shared_ptr<script::ComplexType> configNode) {
		str input=configNode->getS("key");
		secbyteblock out;
		out.resize(input.size());
		size_t outlen=0;
		auto res = sodium_base642bin(_STRTOBYTESIZEOUT(out),_STRTOCHARPSIZE(input),"\t \n\r",&outlen,nullptr,sodium_base64_VARIANT_ORIGINAL);
		if(res==0) {
			out.resize(outlen);
		} else {
			return nullptr;	
		}
		if(out.size()==keySize) {
			return make_shared<crypto::key>(out);
		}
		return nullptr;
	}
	
	/* Store the current encryption key to a configuration string */
	bool storeEncryptionKeyToBlock(script::complextypePtr out) {
		storeKey(_key,out);
		return true;
	}
	
	/* Load the encryption key from a config string */
	bool loadEncryptionKeyFromBlock(script::complextypePtr in) {
		_key = loadKey(in);
		return _key!=nullptr;
	}
	
	str getBucketFileName(int64_t id) {
		//Defines how the protocol creates filenames for "normal" buckets
		str nameInput = str(std::to_string(id).c_str())+":bucket:";
		{
			auto hsh = _filenamesecret->use();
			nameInput.append(_STRTOCHARPSIZE(hsh));
		}
		return crypto::sha256sum(_STRTOBYTESIZE(nameInput)).toLongStr();
	}
	
	str getMetaBucketFileName(int64_t id) {
		//Defines how the protocol creates filename for "hidden" buckets
		str nameInput = str(std::to_string(id).c_str())+":metabucket:";
		{
			auto hsh = _filenamesecret->use();
			nameInput.append(_STRTOCHARPSIZE(hsh));
		}
		return crypto::sha256sum(_STRTOBYTESIZE(nameInput)).toLongStr();
	}
	
	shared_ptr<key> newRandomKey() {
		secbyteblock _realKey;
		_realKey.resize(keySize);
		randombytes_buf(&_realKey[0],keySize);
		return make_shared<crypto::key>(_realKey);
	}
	shared_ptr<key> newRandomIV() {
		secbyteblock _realIV;
		_realIV.resize(IVSize);
		randombytes_buf(&_realIV[0],IVSize);
		return make_shared<crypto::key>(_realIV);
	}
	
	
	void encrypt(shared_ptr<key> k,const str & in, str & out) {
		auto _iv = newRandomIV();
		auto ivbytes = _iv->use();
		out.resize(	in.size()+crypto_aead_xchacha20poly1305_ietf_ABYTES);
		
		unsigned long long output_length=0;
		
		{
			
			auto keybytes = k->use();
			if(crypto_aead_xchacha20poly1305_ietf_encrypt(reinterpret_cast<unsigned char *>(&out[0]), &output_length,
                                   _STRTOBYTESIZE(in),
                                   nullptr, 0, //Optional additional data
                                   NULL, ivbytes.data(), keybytes.data() )) {
				throw std::logic_error("Encryption function return non-zero");
			}
		}
		//CLOG("data:",_data.size()," output_length",output_length," iv size: ",_iv->size()," expected ivSize",ivSize());
		out.resize(output_length);
		
		//Append the IV to the data:
		out.append(reinterpret_cast<const char *>(ivbytes.data()),ivbytes.size());	
	}
	void decrypt(shared_ptr<key> k,const str & in, str & out) {
		str cipher = in.substr(0,in.size()-IVSize);
		//split out the IV:
		const str ivdta = in.substr(in.size()-IVSize);
		_ASSERT(cipher.size()>0);
		out.resize(	cipher.size());
		
		unsigned long long output_length=0;
		
		{
			
			auto keybytes = k->use();
			//CLOG("_data.size()",_data.size()," cipher.size()",cipher.size()," _iv.size()",_iv->size());
			auto res = crypto_aead_xchacha20poly1305_ietf_decrypt(
				reinterpret_cast<unsigned char *>(&out[0]), &output_length, //Output
				nullptr, //Not used
				_STRTOBYTESIZE(cipher),
				nullptr,0,//Optional additional data
				reinterpret_cast<const unsigned char *>(ivdta.data()), keybytes.data());
			if(res!=0) {
				throw std::logic_error("Message forged");
			}
		}
		out.resize(output_length);
	}

	
};


#define _RETURNPROTOCOL(MAJOR,MINOR,ALT) if(major==MAJOR && minor == MINOR && alt == ALT) { return make_unique<protocol<MAJOR,MINOR,ALT>>(config); }
 
unique_ptr<protocolInterface> protocolInterface::get(std::shared_ptr<script::ComplexType> config) {
	str version = config->getS("version");
	bool alt = false;
	if(version.empty())	throw std::logic_error("bad protocol version nr");
	if(version.back()=='b') {alt = true; version.pop_back();}
	if(version.empty())	throw std::logic_error("bad protocol version nr");
	
	
	str_list out;
	if(strSplit(version,".",out)!=2) {
		throw std::logic_error("bad protocol version nr");
	}
	int major = std::stoi(out[0].c_str()), minor = std::stoi(out[1].c_str());

	_RETURNPROTOCOL(1,0,false);
	_RETURNPROTOCOL(1,1,false);	
	throw std::logic_error("protocol version could not be instantiated");
}

std::shared_ptr<script::ComplexType> protocolInterface::newConfig(str version) {
	auto ret = script::ComplexType::newComplex();
	if(version=="latest") {
		version = "1.1";
	} else if(version=="latest_b") {
		version = "1.1b";
	}
	ret->getS("version") = version;
	{
		auto prot =  protocolInterface::get(ret);
		prot->buildNewConfig();
	}
	return ret;
}

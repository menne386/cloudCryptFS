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

using namespace script::SLT;

#define _UNDEFINED() throw std::logic_error(std::string("Protocol undefined for function ")+std::string( __func__) );



template<size_t V, size_t MV,bool VA,bool push>
class stream : public streamInterface {
private:
	crypto_secretstream_xchacha20poly1305_state state;
public:
	stream(shared_ptr<key> k, str & headerOut) : streamInterface() {
		_ASSERT(k->size()>=crypto_secretstream_xchacha20poly1305_KEYBYTES);
		headerOut.resize(crypto_secretstream_xchacha20poly1305_HEADERBYTES);
		auto K = k->use();
		if(push) {
			crypto_secretstream_xchacha20poly1305_init_push(&state, reinterpret_cast<unsigned char *>(&headerOut[0]), reinterpret_cast<unsigned char *>(&K[0]));
		} else {
			if (crypto_secretstream_xchacha20poly1305_init_pull(&state, reinterpret_cast<unsigned char *>(&headerOut[0]), reinterpret_cast<unsigned char *>(&K[0])) != 0) {
				throw std::logic_error("Header invalid, cannot decrypt");
				/* Invalid header, no need to go any further */
			}
		}
	}
	
	void message(const str & in, str & out) override {
		if(push) {
			out.resize(encryptionOverhead(in.size()));
			crypto_secretstream_xchacha20poly1305_push(&state, reinterpret_cast<uint8_t*>(&out[0]), NULL, _STRTOBYTESIZE(in), NULL, 0, 0);
		} else {
			out.resize(in.size() - crypto_secretstream_xchacha20poly1305_ABYTES);
			unsigned char tag;
			if (crypto_secretstream_xchacha20poly1305_pull(&state, reinterpret_cast<uint8_t*>(&out[0]), NULL, &tag, _STRTOBYTESIZE(in), NULL, 0) != 0) {
				/* Invalid/incomplete/corrupted ciphertext - abort */
				throw std::logic_error("Message forged");
			}
		}
	}
	
	size_t encryptionOverhead(size_t messageSize) override{
		return messageSize+crypto_secretstream_xchacha20poly1305_ABYTES;
	}
	
	static size_t headerSize() {
		return crypto_secretstream_xchacha20poly1305_HEADERBYTES;
	}
};


template<size_t V, size_t MV,bool VA>
class protocol : public protocolInterface {
	private:
	shared_ptr<key> _filenamepublic,_filenamesecret,_passwordpublic;	
	public:
	protocol(script::JSONPtr config): protocolInterface(config) {
		if(V==1) {
			//Protocol version 1 uses crypto_aead_xchacha20poly1305_ietf construction
			keySize = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;
			tagSize = 16U;//@todo: does libsodium have a definition of this?
			IVSize = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
			_filenamepublic = loadKey((*config)["filenamepublic"]);
			_passwordpublic = loadKey((*config)["passwordpublic"]);

			(*config)["keyfile"] = "private_key_file.bin";
		} else {
			_UNDEFINED();
		}
	}
	
	str getVersion() override { 
		return getStaticVersion();
	}
	
	static str getStaticVersion() { 
		return BUILDSTRING(V,".",MV,VA?"b":""); 
	}
	
	static bool createIfVersionMatches(const str & in, unique_ptr<protocolInterface> & prot,script::JSONPtr inConfig) {
		if(in == getStaticVersion()) {
			prot = make_unique<protocol<V,MV,VA>>(inConfig);
			return true;
		}
		return false;
	}
	
	void buildNewConfig() override {
		if(V==1) {
			//Set options in config
			_filenamepublic = newRandomKey();
			storeKey(_filenamepublic,(*config)["filenamepublic"]);
			
			//From 1.0 forward we need a password public
			_passwordpublic = newRandomKey();
			storeKey(_passwordpublic,(*config)["passwordpublic"]);
			(*config)["opslimit"] = crypto_pwhash_argon2id_OPSLIMIT_MODERATE;
			(*config)["memlimit"] = crypto_pwhash_argon2id_MEMLIMIT_MODERATE;
		} else {
			_UNDEFINED();
		}
	}
	
	void enterPasswordAndKeyFileContent(const str & input,const str & keyfileContent) override {
		//How the protocol handles entering a password & a keyfile:
		if(V == 1) {
			{
				sha256sum keyfileSum(_STRTOBYTESIZE(keyfileContent)); //Generate sha256 sum of file.
				
				const str fullInput = keyfileSum.toShortStr() + input; //The full password input == base64(keyfileSum) + input;
				 
				_ASSERT(_filenamepublic!=nullptr);
				_ASSERT(_passwordpublic!=nullptr);
				_protokey = expandToKey(_passwordpublic,fullInput);
				_filenamesecret = expandToKey(_filenamepublic, fullInput);
			}
		} else {
			_UNDEFINED();
		}
		_ASSERT(_filenamesecret!=nullptr);
		_ASSERT(_protokey!=nullptr);
	}
	
	void regenerateEncryptionKey(void) override {
		_key = newRandomKey();
	}
	
	/* Store a Key to a JSON object*/
	bool storeKey(shared_ptr<key> k,script::JSONPtr configNode ) override {
		k->toBase64String(configNode->get<S>("key"));
		return true;
	}
	
	shared_ptr<key> loadKey(script::JSONPtr configNode) override {
		str input=(*configNode)["key"];
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

	shared_ptr<key> expandToKey(shared_ptr<key> saltKey,const str & input) override {
		//From version 1.0 & forward is used to generate the keys
		if (V == 1) {
			_ASSERT(saltKey != nullptr);
			_ASSERT(crypto_pwhash_argon2id_SALTBYTES <= saltKey->size());

			secbyteblock _realKey;
			_realKey.resize(keySize, 0);
			auto salt = saltKey->use();
			_ASSERT(crypto_pwhash_argon2id(
				_STRTOBYTESIZEOUT(_realKey),
				_STRTOCHARPSIZE(input),
				salt.data(),
				(*config)["opslimit"], (*config)["memlimit"],
				crypto_pwhash_argon2id_ALG_ARGON2ID13
			) == 0);
			return make_shared<crypto::key>(_realKey);
		} else {
			_UNDEFINED();
		}
	}

	
	/* Store the current encryption key to a configuration string */
	bool storeEncryptionKeyToBlock(script::JSONPtr out) override {
		storeKey(_key,out);
		return true;
	}
	
	/* Load the encryption key from a config string */
	bool loadEncryptionKeyFromBlock(script::JSONPtr in) override {
		_key = loadKey(in);
		return _key!=nullptr;
	}
	
	str getBucketFileName(int64_t id) override {
		//Defines how the protocol creates filenames for "normal" buckets
		str nameInput = str(std::to_string(id).c_str())+":bucket:";
		{
			auto hsh = _filenamesecret->use();
			nameInput.append(_STRTOCHARPSIZE(hsh));
		}
		return crypto::sha256sum(_STRTOBYTESIZE(nameInput)).toLongStr();
	}
	
	str getMetaBucketFileName(int64_t id) override {
		//Defines how the protocol creates filename for "hidden" buckets
		str nameInput = str(std::to_string(id).c_str())+":metabucket:";
		{
			auto hsh = _filenamesecret->use();
			nameInput.append(_STRTOCHARPSIZE(hsh));
		}
		return crypto::sha256sum(_STRTOBYTESIZE(nameInput)).toLongStr();
	}
	
	shared_ptr<key> newRandomKey() override {
		secbyteblock _realKey;
		_realKey.resize(keySize);
		randombytes_buf(&_realKey[0],keySize);
		return make_shared<crypto::key>(_realKey);
	}
	shared_ptr<key> newRandomIV() override {
		secbyteblock _realIV;
		_realIV.resize(IVSize);
		randombytes_buf(&_realIV[0],IVSize);
		return make_shared<crypto::key>(_realIV);
	}
	
	
	void encrypt(shared_ptr<key> k,const str & in, str & out) override {
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
	void decrypt(shared_ptr<key> k,const str & in, str & out) override {
		str cipher = in.substr(0,in.size()-IVSize);
		
		//split out the IV:
		const str ivdta = in.substr(in.size()-IVSize);
		auto * tst = reinterpret_cast<const uint64_t *>(ivdta.data());
		if(*tst == 0) {
			throw std::logic_error("Too many zero's in IV");
		}
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


	str createKeyfileContent(void) override {
		str content;
		for (int a = 0; a < 5; a++) {
			auto _k = newRandomKey();
			auto _ku = _k->use();
			content.append(_STRTOCHARPSIZE(_ku));
		}
		return content;
	}
	
	
	shared_ptr<streamInterface> startStreamWrite(shared_ptr<key> k,str & headerOut) override {
		return make_shared<stream<V,MV,VA,true>>(k,headerOut);
	}
	
	shared_ptr<streamInterface> startStreamRead(shared_ptr<key> k,str & headerOut) override {
		return make_shared<stream<V,MV,VA,false>>(k,headerOut);
	}
	
	
	size_t streamHeaderSize() override {
		return stream<V,MV,VA,false>::headerSize();
	}
	
	
};

unique_ptr<protocolInterface> protocolInterface::get(script::JSONPtr config) {
	const str & version = (*config)["version"];
	unique_ptr<protocolInterface> ret;

	//The list of protocols:	
	if(protocol<1,0,false>::createIfVersionMatches(version,ret,config)) { return ret; }
	if(protocol<0,0,false>::createIfVersionMatches(version,ret,config)) { return ret; }

	throw std::logic_error("protocol version could not be instantiated");
}

script::JSONPtr protocolInterface::newConfig(str version) {
	auto ret = script::make_json();
	if(version=="latest") {
		version = "1.0";
	} else if(version=="latest_b") {
		version = "1.0b";
	}
	(*ret)["version"] = version;
	{
		auto prot =  protocolInterface::get(ret);
		prot->buildNewConfig();
	}
	return ret;
}

// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef CRYPTO_PROTOCOL_H
#define CRYPTO_PROTOCOL_H

#include "types.h"
#include "key.h"

#include "modules/script/script_complex.h"


namespace crypto{
	class protocolInterface{
		private:
		size_t majorVer;
		size_t minorVer;
		bool isAlt;
		
		protected:
		
		std::shared_ptr<script::ComplexType> config;
		size_t keySize=0,IVSize=0,tagSize=0;
		std::shared_ptr<key> _key,_protokey;
		
		
		public:
		
		protocolInterface(size_t iversion,size_t iminor,bool iIsAlt,std::shared_ptr<script::ComplexType> iconfig) : majorVer(iversion),minorVer(iminor),isAlt(iIsAlt), config(iconfig) {}
		virtual ~protocolInterface() {}
		
		/* Get the protocol defined by a configuration set: */
		static unique_ptr<protocolInterface> get(std::shared_ptr<script::ComplexType> config);
		static std::shared_ptr<script::ComplexType> newConfig(str version);

		str getVersion();
		//Return encryption key sizes and such:
		size_t getKeysize() { return keySize; }
		size_t getIVSize() { return IVSize; }
		size_t getTagSize() { return tagSize; }
		
		/* Builds fills the internal config array with protocol specific config data (when creating a new filesystem!) */
		virtual void buildNewConfig()=0;
		
		/* This function is called when user enters password, it will set up proto encryption key and related secrets */
		/* @todo: the protocol should allow for key files, and key files + passwords */
		virtual void enterPassword(const str & input) = 0;
		
		/* Regenerate the main encryption Key */
		virtual void regenerateEncryptionKey(void) = 0;
		
		/* Get encryption key for proto state (metabuckets) */
		shared_ptr<key> getProtoEncryptionKey() { return _protokey;	}
		/* Get encryption key for normal encryption/decryption */
		shared_ptr<key> getEncryptionKey() { return _key; }
		
		/* Store the current encryption key to a configuration string */
		virtual bool storeEncryptionKeyToBlock(script::complextypePtr out) = 0;
		/* Load the encryption key from a config string */
		virtual bool loadEncryptionKeyFromBlock(script::complextypePtr in) = 0;
		
		/* Get a filename to a normal bucket */
		virtual str getBucketFileName(int64_t id) = 0;
		/* Get a filename to a meta-bucket */
		virtual str getMetaBucketFileName(int64_t id) = 0;
		
		/* Generate a new random encryption key */
		virtual shared_ptr<key> newRandomKey() = 0;
		
		/* Generate a new random iv */
		virtual shared_ptr<key> newRandomIV() = 0;
		
		/* Use a key to encrypt data */
		virtual void encrypt(shared_ptr<key> k,const str & in, str & out) = 0;
		
		/* Use a key to decrypt data */
		virtual void decrypt(shared_ptr<key> k,const str & in, str & out) = 0;
		
			
		
	};
	
	

}



#endif


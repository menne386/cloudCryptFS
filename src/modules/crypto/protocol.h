// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef CRYPTO_PROTOCOL_H
#define CRYPTO_PROTOCOL_H

#include "types.h"
#include "key.h"

#include "modules/script/JSON.h"


namespace crypto{
	
	class streamInterface{
		public:
			streamInterface() {};
			virtual ~streamInterface() {};
			
			virtual void message(const str & in, str & out) = 0;
			
			virtual size_t encryptionOverhead(size_t messageSize) = 0;
	};
	
	class protocolInterface{		
		protected:
		
		script::JSONPtr config;
		size_t keySize=0,IVSize=0,tagSize=0;
		std::shared_ptr<key> _key,_protokey;
		
		
		public:
		
		protocolInterface(script::JSONPtr iconfig) : config(iconfig) {}
		virtual ~protocolInterface() {}
		
		/* Get the protocol defined by a configuration set: */
		static unique_ptr<protocolInterface> get(script::JSONPtr config);
		static script::JSONPtr newConfig(str version);

		//Return encryption key sizes and such:
		size_t getKeysize() { return keySize; }
		size_t getIVSize() { return IVSize; }
		size_t getTagSize() { return tagSize; }

		virtual str getVersion() = 0;

		
		/* Builds fills the internal config array with protocol specific config data (when creating a new filesystem!) */
		virtual void buildNewConfig()=0;
		
		/* This function is called when user enters password, it will set up proto encryption key and related secrets */
		/* the protocol should allow for key files + passwords */
		virtual void enterPasswordAndKeyFileContent(const str & input,const str & keyfileContent) = 0;
		
		/* store a key to JSON config node. */
		virtual bool storeKey(shared_ptr<key> k, script::JSONPtr configNode) = 0;

		/* load a key from JSON config node. */
		virtual shared_ptr<key> loadKey(script::JSONPtr configNode) = 0;

		/* Expand a salt + a user input to a key */
		virtual shared_ptr<key> expandToKey(shared_ptr<key> saltKey, const str& input) = 0;

		/* Regenerate the main encryption Key */
		virtual void regenerateEncryptionKey(void) = 0;
		
		/* Get encryption key for proto state (metabuckets) */
		shared_ptr<key> getProtoEncryptionKey() { return _protokey;	}
		/* Get encryption key for normal encryption/decryption */
		shared_ptr<key> getEncryptionKey() { return _key; }
		
		/* Store the current encryption key to a configuration string */
		virtual bool storeEncryptionKeyToBlock(script::JSONPtr out) = 0;

		/* Load the encryption key from a config string */
		virtual bool loadEncryptionKeyFromBlock(script::JSONPtr in) = 0;
		
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
		
		/* Regenerate content for a keyfile */
		virtual str createKeyfileContent(void) = 0;
		
		
		
		virtual shared_ptr<streamInterface> startStreamWrite(shared_ptr<key> k,str & headerOut) = 0;
		virtual shared_ptr<streamInterface> startStreamRead(shared_ptr<key> k,str & headerOut) = 0;
		virtual size_t streamHeaderSize() = 0;

		
	};
	
	

}



#endif


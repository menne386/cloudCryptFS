// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
/**
 * The chunk class represents a fixed size (4096bytes) block of data.
 * all operations are const with the exception of as<T>().
 * The write operation will return a copy of the data with the added changes.
 * 
 * The as<T> operation will is used for interpreting the data as inode/inode_ctd records.
 * 
 */
#include "chunk.h"
#include "modules/crypto/sha256.h"

using namespace filesystem;
#include <stdexcept>

chunk::chunk(){
	zeroOut(data);
}

chunk::~chunk() {
	zeroOut(data);
}
crypto::sha256sum filesystem::chunk::getHash() {
	return crypto::sha256sum(data.data(),data.size());
}




void chunk::read(my_off_t offset,my_size_t size,unsigned char * output) const {
	if(offset+size<=data.size()) {
		std::copy(data.begin()+offset,data.begin()+offset+size,output);
	} else {
		throw std::out_of_range("chunk::read "+std::to_string(offset)+" "+std::to_string(size));
	}
}

std::shared_ptr<chunk> chunk::write(my_off_t offset,my_size_t size,const unsigned char * input) const {
	auto newChunk = std::make_shared<chunk>();
	newChunk->data = data;
	if(offset+size<=data.size()) {
		std::copy(input,input+size,newChunk->data.begin()+offset);
	} else {
		throw std::out_of_range("chunk::write "+std::to_string(offset)+" "+std::to_string(size));
	}
	return newChunk;
}

std::shared_ptr<chunk> chunk::clone() const {
	auto newChunk = std::make_shared<chunk>();
	newChunk->data = data;

	return newChunk;
}


std::shared_ptr<chunk> filesystem::chunk::newChunk(my_size_t size, const unsigned char * input) {
	auto newChunk = std::make_shared<chunk>();
	if(input && size) {
		if ( size <= newChunk->data.size()) {
			std::copy(input, input + size, newChunk->data.begin());
		}
		else {
			throw std::out_of_range("chunk::newChunk " + std::to_string(size));
		}
	}
	return newChunk;
}

bool chunk::compareChunk(shared_ptr<chunk> c) {
	auto * A = reinterpret_cast<uint64_t *>(&data[0]);
	auto * B = reinterpret_cast<uint64_t *>(&c->data[0]);
	for(unsigned i=0;i<data.size()/sizeof(uint64_t);++i,++A,++B) {
		if(*A != *B) {
			//CLOG("ERROR: ",*A ,"!=",*B);
			return false;
		}
	}
	
	return true;
}

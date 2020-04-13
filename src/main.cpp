// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#include "main.h"

#include <string.h>
#include <errno.h>
#include <map>
#include <vector>
#include <functional>
#include <iostream>
#include <fstream>
#ifndef _WIN32
#include <syslog.h>
#endif

#include <modules/filesystem/fs.h>
#include <modules/filesystem/storage.h>
#include <modules/crypto/protocol.h>
#include <modules/script/JSON.h>
#include <modules/util/files.h>
#include <modules/util/console.h>
#include "modules/util/endian.h"
#include "modules/filesystem/journal.h"

bool Fully_up_and_running = false;

static str g_LogFile = "log.txt";

void * parseArgs(int argc,char* argv[], myfs_config * conf);
int _realMain(void * args);

int main(int argc, char *argv[]) {
	util::testEndianFunction();
	
	atexit(services::stop_all_services);
	struct myfs_config conf;
	
	auto * args = parseArgs(argc,argv,&conf);

	if(conf.loglevel) {
		STOR->setLogLevel(std::stoi(conf.loglevel));
		FS->setLogLevel(std::stoi(conf.loglevel));
		JOURNAL->setLogLevel(std::stoi(conf.loglevel));

	}
	
	if(conf.source) {
		//CLOG(conf.source);
		STOR->setPath(conf.source);
		if(STOR->getPath().back()!='/') {
			CLOG("Error: source path should end with '/'");
			exit(EXIT_FAILURE);
		}
		if(STOR->getPath().front()=='.') {
			CLOG("Error: source path should not start with '.'");
			exit(EXIT_FAILURE);
		}
	} 
	
	

	g_LogFile = STOR->getPath()+"log.txt";
	CLOG("Source path: ", STOR->getPath());
	CLOG("LogFile: ",g_LogFile);
	

	

	


	//std::shared_ptr<crypto::key> key,secret;
	bool mustCreate = conf.create && str(conf.create) == "yes";
	
	str cfg = util::getSystemString(STOR->getPath()+"config.json");
	auto config = script::make_json();
	
	if(mustCreate) {
		FS->srvMESSAGE("Generating new startup configuration");
		config = crypto::protocolInterface::newConfig("latest");
		if(cfg.empty()==false) {
			FS->srvERROR("Found existing config.json, abort!");
			return EXIT_FAILURE;
		}
	} else {
		if(cfg.empty()==false) {
			FS->srvMESSAGE("Loading startup configuration");
			FS->srvDEBUG(cfg);
			config->unserialize(cfg);
		} else {
			FS->srvERROR("Could not load config.json, abort!");
			return EXIT_FAILURE;
		}
	}

	str keyfileName = (*config)["keyfile"];
	if (conf.keyfile) {
		keyfileName = conf.keyfile;
	}

	auto protocol = crypto::protocolInterface::get(config);
	if (mustCreate) {
		FS->srvMESSAGE("Storing new keyfile ", keyfileName);
		util::putSystemString(keyfileName, protocol->createKeyfileContent());
		FS->srvMESSAGE("Storing new config ", STOR->getPath() + "config.json");
		util::putSystemString(STOR->getPath() + "config.json", config->serialize(1));
	}
	{
		str passwd,passwd2="2",keyfileContent = util::getSystemString(keyfileName);
		if (keyfileContent.empty()) {
			CLOG("File ", keyfileName, " not found!");
			return EXIT_FAILURE;
		}
		if(conf.password) {
			passwd = conf.password;
			passwd2 = conf.password;
		} else {
			util::getPassWord(passwd);
			if(mustCreate) {
				util::getPassWord(passwd2);
				if(passwd!=passwd2) {
					CLOG("The passphrases are not equal.");
					return EXIT_FAILURE;
				}
			}
		}		
		protocol->enterPasswordAndKeyFileContent(passwd, keyfileContent);
	}
	
	
	if(!FS->initFileSystem(std::move(protocol),mustCreate)) {
		return EXIT_FAILURE;
	}
	
	
	if(conf.migrate) {
		auto newConfig = script::make_json();
		newConfig = crypto::protocolInterface::newConfig(conf.migrate);
		auto newProtocol = crypto::protocolInterface::get(newConfig);
		const str keyfileContent = newProtocol->createKeyfileContent();
		{
			str passwd,passwd2="2";
			if(conf.password) {
				passwd = conf.password;
				passwd2 = conf.password;
			} else {
				util::getPassWord(passwd);
				util::getPassWord(passwd2);
				if(passwd!=passwd2) {
					CLOG("The passphrases are not equal.");
					return EXIT_FAILURE;
				}
			}
			newProtocol->enterPasswordAndKeyFileContent(passwd, keyfileContent);
		}
		FS->migrate(std::move(newProtocol));
		util::putSystemString(keyfileName + ".bak", util::getSystemString(keyfileName));//Backup old keyfile
		util::putSystemString(keyfileName, keyfileContent);
		util::putSystemString(STOR->getPath()+"config.json",newConfig->serialize(1));
		services::stop_all_services();
		return EXIT_SUCCESS;
	}
	
	if(mustCreate) {
		services::stop_all_services();
		return EXIT_SUCCESS;
	}
		
	return _realMain(args);
}


void assertfail(int condition,const char * string) {
	if(!condition) {
		CLOG(string);
		std::abort();
	}
}

void CLOG(const char* MESSAGE) {
	std::ofstream F;
	F.open(g_LogFile.c_str(),std::ios::out|std::ios::app);
	if(F.is_open()) {
		F << time(NULL)<< " " << MESSAGE << std::endl;
		F.close();
	}

	std::cout << time(NULL) << " " << MESSAGE << std::endl;
}

void CLOG(const str & MESSAGE) {
	CLOG(MESSAGE.c_str());
}



#ifdef _WIN32
int getuid() {return 0;}
int getgid() {return 0;}
#endif


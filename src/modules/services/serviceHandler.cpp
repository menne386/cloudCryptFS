// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
/* 
 * File:   serviceHandler.cpp
 * Author: menne
 * 
 * Created on April 7, 2010, 12:26 PM
 */
#define	_SERVICEHANDLER_CPP
#include "serviceHandler.h"

using namespace services;

serviceHandler SH;

service::service (const str & n) : _MSG(n + ": "), _ERR( n + " ERROR: "), _WRN( n + " WARNING: "),_DBG(n+" DEBUG: "),  myName(n), deletemeptr(nullptr) {
	srvMESSAGE("starting");
	//Attach to the main list
	SH.startService(this);
}

service::~service () {
	if(deletemeptr) {
		deletemeptr[0] = true;
	}
	srvMESSAGE("stopping");
	//Remove from the global list.
	SH.stopService(this);
}

void service::attachDeleteMePtr(bool * delptr) {
	deletemeptr = delptr;
}


const char * service::getName(void) {
	return myName.c_str();
}

void service::running(double time) {
	//Nothing to do here!
}

void service::adddebugdata(void) {
	//Nothing to do here!
}

void service::runningInGameTime(double time) {
	//Nothing to do here!
}

/**
 *
 *
 * */
void serviceHandler::stopService(service * service) {
	_ASSERT(service!=nullptr);
	for(unsigned int a=0;a<services.size();a++) {
		if(services[a] == service) {
			//Remove from list:
			services.erase(services.begin()+a);
			return;
		}
	}
	CLOG("ERROR: Failed to stop service! could not find service pointer!");
}

void serviceHandler::startService(service * service) {
	_ASSERT(service!=nullptr);
	services.push_back(service);
}


serviceHandler::serviceHandler () {
	services.clear();
	services.reserve(20);
	//services.push_back(nullptr);
}
void serviceHandler::stopAllServices(void) {
	while(!services.empty()) {
		delete services.back();
	}
}

serviceHandler::~serviceHandler (){
	if(services.empty()==false) {
		CLOG("ERROR: services array not empty.");
	}
	stopAllServices();
}

void serviceHandler::run(double t) {
	for(const auto & service:services) {
		service->running(t);
	}
}

void serviceHandler::runInGameTime(double time) {
	for(const auto & service:services) {
		service->runningInGameTime(time);
	}
}

void serviceHandler::addDebugHudStuff(void) {
	for(const auto & service:services) {
		service->adddebugdata();
	}	
}
namespace services{
void stop_all_services(void) {
	SH.stopAllServices();
}
};

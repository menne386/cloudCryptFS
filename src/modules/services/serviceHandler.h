// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
/* 
 * File:   serviceHandler.h
 * Author: menne
 *
 * Created on April 7, 2010, 12:26 PM
 */

#ifndef _SERVICEHANDLER_H
#define	_SERVICEHANDLER_H

#include "main.h"
#include <vector>
namespace services {

class serviceHandler;


#define srvSTATICDEFAULTNEWINSTANCE(CLASSNAME) static CLASSNAME * srvNewInstance(void) { return new CLASSNAME(); }

class service {
public:
	void attachDeleteMePtr(bool * delptr);
	explicit service (const str & name);
	virtual ~service();
	const char * getName(void);

	template<class ...Args> void srvMESSAGE(const Args& ...args) { if(loglevel>0) CLOG(_MSG, args...); }
	template<class ...Args> void srvERROR(const Args& ...args) { if(loglevel>1) CLOG(_ERR, args...); }
	template<class ...Args> void srvWARNING(const Args& ...args) { if(loglevel>2) CLOG(_WRN, args...); }
	template<class ...Args> void srvDEBUG(const Args& ...args) { if(loglevel>3) CLOG(_DBG, args...); }
	void setLogLevel(int a) {loglevel = a;}

	friend class serviceHandler;
	str _MSG, _ERR, _WRN, _DBG;
protected:
	virtual void runningInGameTime(double time); //This function can be overridden to have a service do stuff each frame... (for example: clean something up each second.)
	virtual void running(double time); //This function can be overridden to have a service do stuff each frame... (for example: clean something up each second.)
	virtual void adddebugdata(void); //This function can be overridden to have a service do stuff each frame...
	str myName;
private:
	bool * deletemeptr;
	int loglevel = 3;

};


#define srvREQUIRESERVICE(SERVICENAME) CLOG("required " , SERVICENAME->getName())

class serviceHandler {
public:
			serviceHandler();
			~serviceHandler();

	void run(double t);
	void runInGameTime(double time);
	void addDebugHudStuff(void);
	void stopAllServices(void);

private:
	friend class service;
	void stopService( service * service);
	void startService( service * service);
	std::vector<service *> services;
};





#ifndef _SERVICEHANDLER_CPP
extern serviceHandler SH;
#endif
//bool isMainThread(void);
/**
 * Use this to create an automaticly starting service all classes should implement a srvNewInstance function to create a new object
 * This function can be used to start other required services before this one.
 * Also: srvSTATICDEFAULTNEWINSTANCE can be used if no other service is required.
 *
 **/
template <class myType,bool mainOnly = false> class t_autoStartService {
private:
	myType * internalPointer;
	bool hasbeendeleted;
public:

	bool getIsRunning(void) {
		return (internalPointer != nullptr && hasbeendeleted == false);
	}
	//Constructors:

	t_autoStartService() {
		internalPointer = nullptr;
		hasbeendeleted = false;
	}

	void stopService(void) {
		if (getIsRunning()) {
			delete internalPointer;
			internalPointer = nullptr;
		}
	}

	//Extraction:

	myType * operator->() {
		if(mainOnly) {
			//_ASSERT(isMainThread()==true);	
		}	
		if (hasbeendeleted) {
			CLOG("Service is required AGAIN after being retired.");
			internalPointer = nullptr;
			hasbeendeleted = false;
		}
		if (internalPointer == nullptr) {
			internalPointer = myType::srvNewInstance();
			if (internalPointer == nullptr) {
				CLOG("Could not start service properly!");
			} else {
				internalPointer->attachDeleteMePtr(&hasbeendeleted);
			}
		}
		return internalPointer;
	}
};
	void stop_all_services(void);
}

#endif	/* _SERVICEHANDLER_H */


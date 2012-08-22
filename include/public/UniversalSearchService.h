// @@@LICENSE
//
//      Copyright (c) 2010-2012 Hewlett-Packard Development Company, L.P.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// LICENSE@@@

#ifndef __UniversalSearchService_h__
#define __UniversalSearchService_h__

#include <string>
#include <algorithm>
#include <stdlib.h>
#include <cstring>
#include <glib.h>
#include <sqlite3.h>
#include <lunaservice.h>
#include <cjson/json.h>

#include "SearchItemsManager.h"
#include "SearchServiceManager.h"
#include "OpenSearchHandler.h"


class UniversalSearchService {
	
public:
	static UniversalSearchService* instance();
	static sqlite3* m_gsDb;
	
	char* trim(char* s);
	
	SearchItemsManager* searchItemsMgr;
	//SearchServiceManager* searchServiceMgr; //Currently, not being used.
	OpenSearchHandler* openSearchHandler;
	
	void postSearchListChange(const char* eventName);
	//void postServiceListChange();
	void postSearchPreferenceChange();
	void postOptionalSearchListChange();
	
	//Application Manager status - static
	static bool cbAppMgrBusStatusNotification(LSHandle* lshandle, LSMessage *message,void *user_data);    
	static bool cbAppMgrAppList(LSHandle* lshandle, LSMessage *message,void *user_data);
	static bool cbAppMgrGetAppInfo(LSHandle* lshandle, LSMessage *message,void *user_data);
	
	//Application Installer status - static
	static bool cbAppInstallerBusStatusNotification(LSHandle* lshandle, LSMessage *message,void *user_data);    
	static bool cbAppInstallerNotifyOnChange(LSHandle* lshandle, LSMessage *message,void *user_data);
	
	//System Service Statics - callback methods
	static bool cbSysServiceBusStatusNotification(LSHandle* lshandle, LSMessage *message,void *user_data);
	static bool cbGetLocalePref(LSHandle* lshandle, LSMessage *message,void *user_data);
	
	std::string getLocale();
	void setLocale(const std::string& locale);
	
	LSHandle* getServiceHandle();
	
	UniversalSearchService();
	~UniversalSearchService();
	
private:
	void startService();
	void stopService();
	void postInit();
		
	std::string m_version;
	std::string m_locale;
	
	LSPalmService * m_service;
	LSHandle * m_serviceHandlePublic;
	LSHandle * m_serviceHandlePrivate;
	GMainLoop* m_mainLoop;
};

#endif

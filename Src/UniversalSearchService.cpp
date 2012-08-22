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

#include "UniversalSearchService.h"
#include "UniversalSearchPrefsDb.h"

#include <unistd.h>
#include <sys/stat.h>
#include "Logging.h"
#include "OpenSearchHandler.h"

#define VERSION	"1.0"
#define MAXOPENSEARCHES 50

extern GMainLoop* gMainLoop;

static UniversalSearchService* s_instance = 0;
static const char* s_logChannel = "UniversalSearchService";

//Luna Bus Functions
static bool cbGetVersion(LSHandle* lshandle, LSMessage *message, void *user_data);
static bool cbGetUniversalSearchList(LSHandle* lshandle, LSMessage *message, void *user_data);
static bool cbUpdateSearchItem(LSHandle* lshandle, LSMessage *message, void *user_data);
static bool cbAddSearchItem(LSHandle* lshandle, LSMessage *message, void *user_data);
static bool cbRemoveSearchItem(LSHandle* lshandle, LSMessage *message, void *user_data);
static bool cbReorderSearchItem(LSHandle* lshandle, LSMessage *message, void *user_data);
//static bool cbGetSearchServiceList(LSHandle* lshandle, LSMessage *message, void *user_data);
//static bool cbAddSearchService(LSHandle* lshandle, LSMessage *message, void *user_data);
//static bool cbUpdateSearchService(LSHandle* lshandle, LSMessage *message, void *user_data);
//static bool cbRemoveSearchService(LSHandle* lshandle, LSMessage *message, void *user_data);
static bool cbGetSearchPreference(LSHandle* lshandle, LSMessage *message, void *user_data);
static bool cbSetSearchPreference(LSHandle* lshandle, LSMessage *message, void *user_data);
static bool cbGetAllSearchPreference(LSHandle* lshandle, LSMessage *message, void *user_data);
static bool cbAddOptionalSearchDesc(LSHandle* lshandle, LSMessage *message, void *user_data);
static bool cbGetOptionalSearchList(LSHandle* lshandle, LSMessage *message, void *user_data);
static bool cbClearOptionalSearchList(LSHandle* lshandle, LSMessage *message, void *user_data);
static bool cbRemoveOptionalSearchItem(LSHandle* lshandle, LSMessage *message, void *user_data);
static bool cbUpdateAllSearchItems(LSHandle* lshandle, LSMessage *message, void *user_data);


static LSMethod s_methods[]  = {
	{ "getVersion",		cbGetVersion},
	{ "getUniversalSearchList", cbGetUniversalSearchList},
	{ "updateSearchItem", cbUpdateSearchItem},
	{ "addSearchItem", cbAddSearchItem},
	{ "removeSearchItem", cbRemoveSearchItem},
	{ "reorderSearchItem", cbReorderSearchItem},
	{ "updateAllSearchItems", cbUpdateAllSearchItems},
//	{ "getSearchServiceList", cbGetSearchServiceList},
//	{ "addSearchService", cbAddSearchService},
//	{ "updateSearchService", cbUpdateSearchService},
//	{ "removeSearchService", cbRemoveSearchService},
	{ "getSearchPreference", cbGetSearchPreference},
	{ "getAllSearchPreference", cbGetAllSearchPreference},
	{ "setSearchPreference", cbSetSearchPreference},
	{ "addOptionalSearchDesc", cbAddOptionalSearchDesc},
	{ "getOptionalSearchList", cbGetOptionalSearchList},
	{ "clearOptionalSearchList", cbClearOptionalSearchList},
	{ "removeOptionalSearchItem", cbRemoveOptionalSearchItem},
	{0,0}
};

UniversalSearchService::UniversalSearchService()
{
	m_mainLoop = gMainLoop;

	this->startService();
}

UniversalSearchService::~UniversalSearchService() 
{
	this->stopService();
}

UniversalSearchService* UniversalSearchService::instance() {
	
	if(!s_instance) {
		s_instance = new UniversalSearchService();
	}
	return s_instance;
}

void UniversalSearchService::postInit() {
	bool result;
	LSError lsError;
	LSErrorInit(&lsError);

	result = LSCall(m_serviceHandlePrivate, "palm://com.palm.bus/signal/registerServerStatus",
			    "{\"serviceName\":\"com.palm.applicationManager\", \"subscribe\":true}",
			    UniversalSearchService::cbAppMgrBusStatusNotification, this, NULL, &lsError);

	if (!result) {
		luna_critical(s_logChannel, "unable to register for application manager notification");
		LSErrorPrint (&lsError, stderr);
		LSErrorFree (&lsError);
	}

	//Subscribe for luna bus registration notification for com.palm.appinstaller
	result = LSCall(m_serviceHandlePrivate, "palm://com.palm.bus/signal/registerServerStatus",
		    "{\"serviceName\":\"com.palm.appinstaller\", \"subscribe\":true}",
		    UniversalSearchService::cbAppInstallerBusStatusNotification, this, NULL, &lsError);

	if (!result) {
		luna_critical(s_logChannel, "unable to register for applicaton installer notification");
		LSErrorPrint (&lsError, stderr);
		LSErrorFree (&lsError);
	}
}

void UniversalSearchService::startService() {
	bool result;
	LSError lsError;
	LSErrorInit(&lsError);

	result = LSRegisterPalmService("com.palm.universalsearch", &m_service, &lsError);
	if (!result)
		goto Done;

	result = LSPalmServiceRegisterCategory(m_service, "/", s_methods,NULL, NULL, NULL, &lsError);
	if (!result)
		goto Done;
	
	result = LSGmainAttachPalmService(m_service, m_mainLoop, &lsError);
	if (!result)
		goto Done;

	m_serviceHandlePublic = LSPalmServiceGetPublicConnection(m_service);
	m_serviceHandlePrivate = LSPalmServiceGetPrivateConnection(m_service);
	
	//Instantiate the Preference handler
	searchItemsMgr = SearchItemsManager::instance();

	//Instantiate Search Service Manager -- Not being used. Commenting out this one for now....
	//searchServiceMgr = SearchServiceManager::instance();

	//Instantiate Open Search Handler;
	openSearchHandler = OpenSearchHandler::instance();

	result = LSCall(m_serviceHandlePrivate, "palm://com.palm.bus/signal/registerServerStatus",
			    "{\"serviceName\":\"com.palm.systemservice\", \"subscribe\":true}",
			    UniversalSearchService::cbSysServiceBusStatusNotification, this, NULL, &lsError);

	if (!result) {
		luna_critical(s_logChannel, "unable to register for system service notification");
		LSErrorPrint (&lsError, stderr);
		LSErrorFree (&lsError);
	}

		
	Done:
		if (!result) {
			luna_critical(s_logChannel, "Unable to start Universal Search Service");
			LSErrorFree(&lsError);
		}
}

void UniversalSearchService::stopService() {
	LSError lsError;
	LSErrorInit(&lsError);
	bool result;

	result = LSUnregisterPalmService(m_service, &lsError);
	if (!result)
		LSErrorFree(&lsError);

	m_service = 0;
}

LSHandle* UniversalSearchService::getServiceHandle()
{
	return m_serviceHandlePrivate;
}

std::string UniversalSearchService::getLocale() 
{
	return m_locale;
}

void UniversalSearchService::setLocale(const std::string& locale) 
{
	m_locale = locale;
}

bool cbGetVersion(LSHandle* lshandle, LSMessage *message, void *user_data) {

	LSError lserror;
	std::string result;
	
	LSErrorInit(&lserror);
		
	json_object* response = json_object_new_object();
	json_object_object_add (response, "returnValue", json_object_new_boolean (true));
	json_object_object_add (response, "version", json_object_new_string(VERSION));
	
	if (!LSMessageReply( lshandle, message, json_object_to_json_string (response), &lserror )) 	{
	    LSErrorPrint (&lserror, stderr);
	    LSErrorFree(&lserror);
	}
	
	json_object_put(response);
	return true;
}

bool cbGetUniversalSearchList(LSHandle* lshandle, LSMessage *message, void *user_data) {
	LSError lserror;
	std::string result;
	bool subscribed = false;
	std::string defaultSearchEngineValue;
		
	LSErrorInit(&lserror);
			
	json_object* response = json_object_new_object();
	json_object_object_add (response, "returnValue", json_object_new_boolean (true));
	json_object_object_add(response, (char*) "UniversalSearchList", UniversalSearchService::instance()->searchItemsMgr->getSearchList());
	json_object_object_add(response, (char*) "ActionList", UniversalSearchService::instance()->searchItemsMgr->getActionProvidersList());
	json_object_object_add(response, (char*) "DBSearchItemList", UniversalSearchService::instance()->searchItemsMgr->getDBSearchItemList());
	
	defaultSearchEngineValue = UniversalSearchPrefsDb::instance()->getSearchPreference("defaultSearchEngine");
	json_object_object_add (response, "defaultSearchEngine", json_object_new_string (defaultSearchEngineValue.c_str()));
	
	
	if (LSMessageIsSubscription(message)) {		
		if (!LSSubscriptionAdd(lshandle, "getUniversalSearchList",
				message, &lserror)) {
			LSErrorFree(&lserror);
			subscribed = false;
		}
		else 
			subscribed = true;
	}
	
	json_object_object_add(response, (char*) "subscribed", json_object_new_boolean(subscribed));
	
	if (!LSMessageReply( lshandle, message, json_object_to_json_string (response), &lserror )) 	{
	    LSErrorPrint (&lserror, stderr);
	    LSErrorFree(&lserror);
	}
	
	json_object_put(response);
	return true;
}

bool cbUpdateSearchItem(LSHandle* lshandle, LSMessage *message, void *user_data) {
	LSError lserror;
	json_object* response = json_object_new_object();
	std::string category;
	bool success = true;
	
	
	LSErrorInit(&lserror);
	json_object* label = NULL;
	json_object* root = NULL;
	const char* payload = LSMessageGetPayload(message);
	if(!payload) {
		luna_critical(s_logChannel, "Payload is missing");
		success = false;
		goto Done;
	}
		
	root = json_tokener_parse(payload);
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "unable to parse the json message");
		success = false;
		goto Done;
	}
	
	//check the category key
	label = json_object_object_get(root, "category");
	if(!label || is_error(label)) {
		luna_critical(s_logChannel, "category field is missing");
		success = false;
		goto Done;
	}
	
	category = json_object_get_string(label);
	
	if(category.compare("search") == 0)
		success = UniversalSearchService::instance()->searchItemsMgr->modifySearchItem(payload);
	else if(category.compare("action") == 0)
		success = UniversalSearchService::instance()->searchItemsMgr->modifyActionProvider(payload);
	else if(category.compare("dbsearch") == 0)
		success = UniversalSearchService::instance()->searchItemsMgr->modifyDBSearchItem(payload);
	else
		success = false;
	
	Done:
		if(!success) {
			json_object_object_add (response, "returnValue", json_object_new_boolean (false));
			json_object_object_add (response, "errorMessage", json_object_new_string("Unable to get data"));
		}
		else {
			json_object_object_add (response, "returnValue", json_object_new_boolean (true));
		}
		if (!LSMessageReply( lshandle, message, json_object_to_json_string (response), &lserror )) 	{
				LSErrorPrint (&lserror, stderr);
				LSErrorFree(&lserror);
		}
		
		if(success) {
			luna_critical(s_logChannel, "Posting change notificaiton");
			UniversalSearchService::instance()->postSearchListChange("update");
			if(category.compare("search") == 0)
				UniversalSearchService::instance()->postOptionalSearchListChange();
		}
		
		if (root && !is_error(root))
			json_object_put(root);
		
		if (label && !is_error(label))
			json_object_put(label);
		
		json_object_put(response);
		return true;
		
}

bool cbUpdateAllSearchItems(LSHandle* lshandle, LSMessage *message, void *user_data) {
	LSError lserror;
	json_object* response = json_object_new_object();
	std::string category;
	bool success = true;


	LSErrorInit(&lserror);
	json_object* label = NULL;
	json_object* root = NULL;
	const char* payload = LSMessageGetPayload(message);
	if(!payload) {
		luna_critical(s_logChannel, "Payload is missing");
		success = false;
		goto Done;
	}

	root = json_tokener_parse(payload);
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "unable to parse the json message");
		success = false;
		goto Done;
	}

	//check the category key
	label = json_object_object_get(root, "category");
	if(!label || is_error(label)) {
		luna_critical(s_logChannel, "category field is missing");
		success = false;
		goto Done;
	}

	category = json_object_get_string(label);

	if(category.compare("search") == 0)
		success = UniversalSearchService::instance()->searchItemsMgr->modifyAllSearchItems(payload);
	else if(category.compare("action") == 0)
		success = UniversalSearchService::instance()->searchItemsMgr->modifyAllActionProviders(payload);
	else if(category.compare("dbsearch") == 0)
		success = UniversalSearchService::instance()->searchItemsMgr->modifyAllDBSearchItems(payload);
	else
		success = false;

	Done:
		if(!success) {
			json_object_object_add (response, "returnValue", json_object_new_boolean (false));
			json_object_object_add (response, "errorMessage", json_object_new_string("Unable to get data"));
		}
		else {
			json_object_object_add (response, "returnValue", json_object_new_boolean (true));
		}
		if (!LSMessageReply( lshandle, message, json_object_to_json_string (response), &lserror )) 	{
				LSErrorPrint (&lserror, stderr);
				LSErrorFree(&lserror);
		}

		if(success) {
			luna_critical(s_logChannel, "Posting change notificaiton");
			UniversalSearchService::instance()->postSearchListChange("update");
			/*if(category.compare("search") == 0)
				UniversalSearchService::instance()->postOptionalSearchListChange();*/
		}

		if (root && !is_error(root))
			json_object_put(root);

		if (label && !is_error(label))
			json_object_put(label);

		json_object_put(response);
		return true;


}

bool cbAddSearchItem(LSHandle* lshandle, LSMessage *message, void *user_data) {
	LSError lserror;
	json_object* response = json_object_new_object();
	std::string category;
	bool success = true;
			
	LSErrorInit(&lserror);
	json_object* label = NULL;
	json_object* root = NULL;
	
	const char* payload = LSMessageGetPayload(message);
	
	if(!payload) {
		luna_critical(s_logChannel, "Payload is missing");
		success = false;
		goto Done;
	}
	
	root = json_tokener_parse(payload);
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "unable to parse the json message");
		success = false;
		goto Done;
	}

	//check the category key
	label = json_object_object_get(root, "category");
	if(!label || is_error(label)) {
		luna_critical(s_logChannel, "category field is missing");
		success = false;
		goto Done;
	}
			
	category = json_object_get_string(label);
			
	if(category.compare("search") == 0)
		success = UniversalSearchService::instance()->searchItemsMgr->addSearchItem(payload, true, false,true);
	else if(category.compare("action") == 0)
		success = UniversalSearchService::instance()->searchItemsMgr->addActionProvider(payload, true, false, true);
	else if(category.compare("dbsearch") == 0)
		success = UniversalSearchService::instance()->searchItemsMgr->addDBSearchItem(payload, true, false,true);
	else
		success = false;
		
	Done:
		if(!success) {
			json_object_object_add (response, "returnValue", json_object_new_boolean (false));
			json_object_object_add (response, "errorMessage", json_object_new_string("Unable to add item"));
		}
		else {
			json_object_object_add (response, "returnValue", json_object_new_boolean (true));
		}
		if (!LSMessageReply( lshandle, message, json_object_to_json_string (response), &lserror )) 	{
			LSErrorPrint (&lserror, stderr);
			LSErrorFree(&lserror);
		}
			
		if(success) {
			luna_critical(s_logChannel, "Posting change notificaiton");
			UniversalSearchService::instance()->postSearchListChange("add");
			
			if(category.compare("search") == 0)
				UniversalSearchService::instance()->postOptionalSearchListChange();
		}
		
		if (label && !is_error(label))
			json_object_put(label);
		
		json_object_put(response);
		
	return true;
}

bool cbRemoveSearchItem(LSHandle* lshandle, LSMessage *message, void *user_data) {
	LSError lserror;
	json_object* response = json_object_new_object();
	std::string category;
	bool success = true;
				
	LSErrorInit(&lserror);
	json_object* label = NULL;
	json_object* root = NULL;
	
	const char* payload = LSMessageGetPayload(message);
		
	if(!payload) {
		luna_critical(s_logChannel, "Payload is missing");
		goto Done;
	}
	
	root = json_tokener_parse(payload);
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "unable to parse the json message");
		success = false;
		goto Done;
	}
	
	//check the category key
	label = json_object_object_get(root, "category");
	if(!label || is_error(label)) {
		luna_critical(s_logChannel, "category field is missing");
		success = false;
		goto Done;
	}
		
	category = json_object_get_string(label);
		
	if(category.compare("search") == 0)
		success = UniversalSearchService::instance()->searchItemsMgr->removeSearchItem(payload);
	else if(category.compare("action") == 0)
		success = UniversalSearchService::instance()->searchItemsMgr->removeActionProvider(payload);
	else if(category.compare("dbsearch") == 0)
		success = UniversalSearchService::instance()->searchItemsMgr->removeDBSearchItem(payload);
	else
		success = false;
		
	Done:
		if(!success) {
			json_object_object_add (response, "returnValue", json_object_new_boolean (false));
			json_object_object_add (response, "errorMessage", json_object_new_string("Unable to remove item"));
		}
		else {
			json_object_object_add (response, "returnValue", json_object_new_boolean (true));
		}
		if (!LSMessageReply( lshandle, message, json_object_to_json_string (response), &lserror )) 	{
			LSErrorPrint (&lserror, stderr);
			LSErrorFree(&lserror);
		}
		json_object_put (response);
			
		if(success) {
			luna_critical(s_logChannel, "Posting change notificaiton");
			UniversalSearchService::instance()->postSearchListChange("remove");
			if(category.compare("search") == 0)
				UniversalSearchService::instance()->postOptionalSearchListChange();
		}
		
		if (label && !is_error(label))
			json_object_put(label);
		
		
	return true;
}

bool cbReorderSearchItem(LSHandle* lshandle, LSMessage *message, void *user_data) 
{
	LSError lserror;
	json_object* response = json_object_new_object();
	std::string category;
	bool success = true;
				
	LSErrorInit(&lserror);
	json_object* label = NULL;
	json_object* root = NULL;
	
	const char* payload = LSMessageGetPayload(message);
		
	if(!payload) {
		luna_critical(s_logChannel, "Payload is missing");
		goto Done;
	}
	
	root = json_tokener_parse(payload);
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "unable to parse the json message");
		success = false;
		goto Done;
	}
	
	//check the category key
	label = json_object_object_get(root, "category");
	if(!label || is_error(label)) {
		luna_critical(s_logChannel, "category field is missing");
		success = false;
		goto Done;
	}
		
	category = json_object_get_string(label);
		
	if(category.compare("search") == 0)
		success = UniversalSearchService::instance()->searchItemsMgr->reorderSearchItem(payload);
	else if(category.compare("action") == 0)
		success = UniversalSearchService::instance()->searchItemsMgr->reorderActionProvider(payload);
	else if(category.compare("dbsearch") == 0)
		success = UniversalSearchService::instance()->searchItemsMgr->reorderDBSearchItem(payload);
	else
		success = false;
		
	Done:
		if(!success) {
			json_object_object_add (response, "returnValue", json_object_new_boolean (false));
			json_object_object_add (response, "errorMessage", json_object_new_string("Unable to reorder item"));
		}
		else {
			json_object_object_add (response, "returnValue", json_object_new_boolean (true));
		}
		if (!LSMessageReply( lshandle, message, json_object_to_json_string (response), &lserror )) 	{
			LSErrorPrint (&lserror, stderr);
			LSErrorFree(&lserror);
		}
		json_object_put (response);
			
		if(success) {
			luna_critical(s_logChannel, "Posting change notificaiton");
			UniversalSearchService::instance()->postSearchListChange("reorder");
		}
		
		if (label && !is_error(label))
			json_object_put(label);
		
	return true;

}

void UniversalSearchService::postSearchListChange(const char* eventName)
{
	LSSubscriptionIter *iter=NULL;
	LSError lserror;
	LSHandle * lsHandle;
		
	LSErrorInit(&lserror);
	
	std::string defaultSearchEngineValue;
	
	json_object* response = json_object_new_object();
	json_object_object_add (response, "returnValue", json_object_new_boolean (true));
	json_object_object_add(response, (char*) "UniversalSearchList", UniversalSearchService::instance()->searchItemsMgr->getSearchList());
	json_object_object_add(response, (char*) "ActionList", UniversalSearchService::instance()->searchItemsMgr->getActionProvidersList());
	json_object_object_add(response, (char*) "DBSearchItemList", UniversalSearchService::instance()->searchItemsMgr->getDBSearchItemList());
	
	defaultSearchEngineValue = UniversalSearchPrefsDb::instance()->getSearchPreference("defaultSearchEngine");
	json_object_object_add (response, "defaultSearchEngine", json_object_new_string (defaultSearchEngineValue.c_str()));
	
	json_object_object_add (response, "event", json_object_new_string (eventName));

	// Find out which handle this subscription needs to go to
	bool retVal = LSSubscriptionAcquire(m_serviceHandlePrivate, "getUniversalSearchList", &iter, &lserror);
	if (retVal) {
		lsHandle = m_serviceHandlePrivate;
		while (LSSubscriptionHasNext(iter)) {
			LSMessage *message = LSSubscriptionNext(iter);
			if (!LSMessageReply(lsHandle,message,json_object_to_json_string (response),&lserror)) {
				LSErrorPrint(&lserror,stderr);
				LSErrorFree(&lserror);
			}
		}

		LSSubscriptionRelease(iter);
	}
	else {
		LSErrorFree(&lserror);
	}
	
	json_object_put(response);
}

bool cbGetSearchPreference(LSHandle* lshandle, LSMessage *message, void *user_data)
{
	LSError lserror;
	std::string key;
	std::string value;
	bool success = true;
		
	LSErrorInit(&lserror);
			
	json_object* response = json_object_new_object();
	json_object* root = NULL;
	json_object* label = NULL;
	
	const char* payload = LSMessageGetPayload(message);
		
	if(!payload) {
		luna_critical(s_logChannel, "Payload is missing");
		success = false;
		goto Done;
	}
	
	root = json_tokener_parse(payload);
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "unable to parse the json message");
		success = false;
		goto Done;
	}
	
	//check the category key
	label = json_object_object_get(root, "key");
	if(!label || is_error(label)) {
		luna_critical(s_logChannel, "key field is missing");
		success = false;
		goto Done;
	}
	key = json_object_get_string(label);
	
	value = UniversalSearchPrefsDb::instance()->getSearchPreference(key);
	
	json_object_object_add (response, (char*)key.c_str(), json_object_new_string (value.c_str()));
	
	Done:
		
		json_object_object_add (response, "returnValue", json_object_new_boolean (success));
		
		if (!LSMessageReply( lshandle, message, json_object_to_json_string (response), &lserror )) 	{
	    	LSErrorPrint (&lserror, stderr);
	    	LSErrorFree(&lserror);
		}
	
		json_object_put(response);
	return true;

}

bool cbGetAllSearchPreference(LSHandle* lshandle, LSMessage *message, void *user_data)
{
	LSError lserror;
	std::string key;
	std::string value;
	bool subscribed = false;
	bool success = true;
		
	LSErrorInit(&lserror);
			
	json_object* response = json_object_new_object();
	json_object* root = NULL;
	
	const char* payload = LSMessageGetPayload(message);
		
	if(!payload) {
		luna_critical(s_logChannel, "Payload is missing");
		success = false;
		goto Done;
	}
	
	root = json_tokener_parse(payload);
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "unable to parse the json message");
		success = false;
		goto Done;
	}
	value = UniversalSearchPrefsDb::instance()->getAllSearchPreference(response);

	if (LSMessageIsSubscription(message)) {		
		if (!LSSubscriptionAdd(lshandle, "getAllSearchPreference",
				message, &lserror)) {
			LSErrorFree(&lserror);
			subscribed = false;
		}
		else 
			subscribed = true;
	}
	
	json_object_object_add(response, (char*) "subscribed", json_object_new_boolean(subscribed));
	
	Done:
		
		json_object_object_add (response, (char*) "returnValue", json_object_new_boolean (success));
		
		if (!LSMessageReply( lshandle, message, json_object_to_json_string (response), &lserror )) 	{
	    	LSErrorPrint (&lserror, stderr);
	    	LSErrorFree(&lserror);
		}
	
		json_object_put(response);
	return true;

	
}

bool cbSetSearchPreference(LSHandle* lshandle, LSMessage *message, void *user_data)
{
	LSError lserror;
	std::string key;
	std::string value;
	bool success = true;
		
	LSErrorInit(&lserror);
			
	json_object* response = json_object_new_object();
	json_object* root = NULL;
	json_object* label = NULL;
	
	const char* payload = LSMessageGetPayload(message);
		
	if(!payload) {
		luna_critical(s_logChannel, "Payload is missing");
		success = false;
		goto Done;
	}
	
	root = json_tokener_parse(payload);
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "unable to parse the json message");
		success = false;
		goto Done;
	}
	
	//check the key
	label = json_object_object_get(root, "key");
	if(!label || is_error(label)) {
		luna_critical(s_logChannel, "key field is missing");
		success = false;
		goto Done;
	}
	key = json_object_get_string(label);
	
	label = json_object_object_get(root, "value");
	if(!label || is_error(label)) {
		luna_critical(s_logChannel, "value field is missing");
		success = false;
		goto Done;
	}
	value = json_object_get_string(label);
	
	success = UniversalSearchPrefsDb::instance()->setSearchPreference(key, value);

	
	Done:
		
		json_object_object_add (response, "returnValue", json_object_new_boolean (success));
		
		if (!LSMessageReply( lshandle, message, json_object_to_json_string (response), &lserror )) 	{
	    	LSErrorPrint (&lserror, stderr);
	    	LSErrorFree(&lserror);
		}
		
		if(success) 
			UniversalSearchService::instance()->postSearchPreferenceChange();
	
		json_object_put(response);
	return true;

}

void UniversalSearchService::postSearchPreferenceChange() 
{
	LSSubscriptionIter *iter=NULL;
	LSError lserror;
	LSHandle * lsHandle;
		
	LSErrorInit(&lserror);
	
	json_object* response = json_object_new_object();
	json_object_object_add (response, "returnValue", json_object_new_boolean (true));
	
	UniversalSearchPrefsDb::instance()->getAllSearchPreference(response);
	
	// Find out which handle this subscription needs to go to
	bool retVal = LSSubscriptionAcquire(m_serviceHandlePrivate, "getAllSearchPreference", &iter, &lserror);
	if (retVal) {
		lsHandle = m_serviceHandlePrivate;
		while (LSSubscriptionHasNext(iter)) {
			LSMessage *message = LSSubscriptionNext(iter);
			if (!LSMessageReply(lsHandle,message,json_object_to_json_string (response),&lserror)) {
				LSErrorPrint(&lserror,stderr);
				LSErrorFree(&lserror);
			}
		}

		LSSubscriptionRelease(iter);
	}
	else {
		LSErrorFree(&lserror);
	}
	
	json_object_put(response);

}
/*
void UniversalSearchService::postServiceListChange() 
{
	LSSubscriptionIter *iter=NULL;
	LSError lserror;
	LSHandle * lsHandle;
		
	LSErrorInit(&lserror);
	
	json_object* response = json_object_new_object();
	json_object_object_add (response, "returnValue", json_object_new_boolean (true));
	json_object_object_add(response, (char*) "searchServiceList", UniversalSearchService::instance()->searchServiceMgr->getSearchServiceList());
	
	// Find out which handle this subscription needs to go to
	bool retVal = LSSubscriptionAcquire(m_serviceHandlePrivate, "getSearchServiceList", &iter, &lserror);
	if (retVal) {
		lsHandle = m_serviceHandlePrivate;
		while (LSSubscriptionHasNext(iter)) {
			LSMessage *message = LSSubscriptionNext(iter);
			if (!LSMessageReply(lsHandle,message,json_object_to_json_string (response),&lserror)) {
				LSErrorPrint(&lserror,stderr);
				LSErrorFree(&lserror);
			}
		}

		LSSubscriptionRelease(iter);
	}
	else {
		LSErrorFree(&lserror);
	}
	
	json_object_put(response);
}

bool cbGetSearchServiceList(LSHandle* lshandle, LSMessage *message, void *user_data) 
{
	LSError lserror;
	bool subscribed = false;
			
	LSErrorInit(&lserror);
				
	json_object* response = json_object_new_object();
	json_object_object_add (response, "returnValue", json_object_new_boolean (true));
	json_object_object_add(response, (char*) "searchServiceList", UniversalSearchService::instance()->searchServiceMgr->getSearchServiceList());
		
	if (LSMessageIsSubscription(message)) {		
		if (!LSSubscriptionAdd(lshandle, "getSearchServiceList",
				message, &lserror)) {
			LSErrorFree(&lserror);
			subscribed = false;
		}
		else 
			subscribed = true;
	}
		
	json_object_object_add(response, (char*) "subscribed", json_object_new_boolean(subscribed));
		
	if (!LSMessageReply( lshandle, message, json_object_to_json_string (response), &lserror )) 	{
	    LSErrorPrint (&lserror, stderr);
	    LSErrorFree(&lserror);
	}

	json_object_put(response);
	return true;
}

bool cbAddSearchService(LSHandle* lshandle, LSMessage *message, void *user_data) 
{
	LSError lserror;
	json_object* response = json_object_new_object();
	std::string id;
	bool res = false;
				
	LSErrorInit(&lserror);
		
	const char* payload = LSMessageGetPayload(message);
	
	if(!payload) {
		luna_critical(s_logChannel, "Payload is missing");
		goto Done;
	}
		
	res = UniversalSearchService::instance()->searchServiceMgr->addSearchServiceInfo(payload);
			
	Done:
		if(!payload || !res) {
			json_object_object_add (response, "returnValue", json_object_new_boolean (false));
			json_object_object_add (response, "errorMessage", json_object_new_string("Unable to add item"));
		}
		else {
			json_object_object_add (response, "returnValue", json_object_new_boolean (true));
		}
		if (!LSMessageReply( lshandle, message, json_object_to_json_string (response), &lserror )) 	{
			LSErrorPrint (&lserror, stderr);
			LSErrorFree(&lserror);
		}
		json_object_put (response);
				
		if(res) {
			luna_critical(s_logChannel, "Posting change notificaiton");
			UniversalSearchService::instance()->postServiceListChange();
		}
		
		json_object_put(response);
		return true;
}

static bool cbUpdateSearchService(LSHandle* lshandle, LSMessage *message, void *user_data)
{
	LSError lserror;
	json_object* response = json_object_new_object();
	std::string id;
	bool res = false;
					
	LSErrorInit(&lserror);
			
	const char* payload = LSMessageGetPayload(message);
		
	if(!payload) {
		luna_critical(s_logChannel, "Payload is missing");
		goto Done;
	}
			
	res = UniversalSearchService::instance()->searchServiceMgr->modifySearchServiceInfo(payload);
				
	Done:
		if(!payload || !res) {
			json_object_object_add (response, "returnValue", json_object_new_boolean (false));
			json_object_object_add (response, "errorMessage", json_object_new_string("Unable to update service info"));
		}
		else {
			json_object_object_add (response, "returnValue", json_object_new_boolean (true));
		}
		if (!LSMessageReply( lshandle, message, json_object_to_json_string (response), &lserror )) 	{
			LSErrorPrint (&lserror, stderr);
			LSErrorFree(&lserror);
		}
		json_object_put (response);
				
		if(res) {
			luna_critical(s_logChannel, "Posting change notificaiton");
			UniversalSearchService::instance()->postServiceListChange();
		}
		
		json_object_put(response);
		return true;
}

static bool cbRemoveSearchService(LSHandle* lshandle, LSMessage *message, void *user_data)
{
	LSError lserror;
	json_object* response = json_object_new_object();
	std::string id;
	bool res = false;
					
	LSErrorInit(&lserror);
			
	const char* payload = LSMessageGetPayload(message);
		
	if(!payload) {
		luna_critical(s_logChannel, "Payload is missing");
		goto Done;
	}
			
	res = UniversalSearchService::instance()->searchServiceMgr->removeSearchServiceInfo(payload);
				
	Done:
		if(!payload || !res) {
			json_object_object_add (response, "returnValue", json_object_new_boolean (false));
			json_object_object_add (response, "errorMessage", json_object_new_string("Unable to remove service info"));
		}
		else {
			json_object_object_add (response, "returnValue", json_object_new_boolean (true));
		}
		if (!LSMessageReply( lshandle, message, json_object_to_json_string (response), &lserror )) 	{
			LSErrorPrint (&lserror, stderr);
			LSErrorFree(&lserror);
		}
		json_object_put (response);
				
		if(res) {
			luna_critical(s_logChannel, "Posting change notificaiton");
			UniversalSearchService::instance()->postServiceListChange();
		}
	return true;
}
*/
bool UniversalSearchService::cbSysServiceBusStatusNotification(LSHandle* lshandle, LSMessage *message, void *user_data)
{
	LSError lsError;
	LSErrorInit(&lsError);
	
	json_object * root = 0;
	const char* payload = LSMessageGetPayload(message);
	if( !payload )
		return false;
	
	root = json_tokener_parse(payload);
	if ((root == NULL) || (is_error(root))) {
		root = NULL;
		return true;
	}

	json_object * label = json_object_object_get(root,"connected");
	if (label != NULL)
	{
		if (json_object_get_boolean(label) == true) 
		{
			//the application manager is on the bus...make a call to receive list of installed apps.
			 if (LSCall(UniversalSearchService::instance()->m_serviceHandlePrivate,"palm://com.palm.systemservice/getPreferences",
			    		"{\"subscribe\":true, \"keys\": [ \"locale\"]}",
			    		UniversalSearchService::cbGetLocalePref,NULL,NULL, &lsError) == false) {
				luna_critical(s_logChannel, "call to systemservice/getPreferences(locale) failed");
				LSErrorFree(&lsError);
			} 
		}
	}
	else 
	{
		luna_critical(s_logChannel, "Registration Status message parsing error");
	}
	
	if (root && !is_error(root))
		json_object_put(root);
	
	return true;
	
}

bool UniversalSearchService::cbGetLocalePref(LSHandle* lshandle, LSMessage *message,void *user_data)
{
	LSError lserror;
	LSErrorInit(&lserror);
	
	json_object* label = NULL; 
	json_object* root = NULL;
	json_object* value = NULL;
	
	const char* languageCode = NULL;
	const char* countryCode = NULL;
	std::string newLocale;
	std::string newLocaleRegion;
	
	bool success = true;
	
	const char* payload = LSMessageGetPayload(message);
	if( !payload ) {
		success = false;
		goto Done;
	}

	root = json_tokener_parse(payload);
	if (!root || is_error(root)) {
		success = false;
		goto Done;
	}
	
	value = json_object_object_get(root, "locale");
	if ((value) && (!is_error(value))) {
		
		label = json_object_object_get(value, "languageCode");
		if ((label) && (!is_error(label))) {
			languageCode = json_object_get_string(label);
		}

		label = json_object_object_get(value, "countryCode");
		if ((label) && (!is_error(label))) {
			countryCode = json_object_get_string(label);
		}

		newLocale = languageCode;
		newLocale += "_";
		newLocale += countryCode;
	}
	else {
		success = false;
		goto Done;
	}
	
	if(newLocale == "_") {
		success = false;
		goto Done;
	}
	
	Done:
	
		if(!success)
			newLocale = "en_us"; //default locale...
		
		if (root && !is_error(root))
			json_object_put(root);
	
		//First time query. m_locale is empty.
		if(UniversalSearchService::instance()->getLocale().empty()) {
			UniversalSearchService::instance()->setLocale(newLocale);
			luna_critical(s_logChannel, "Got the locale %s --- calling init functions ", newLocale.c_str());
			//Call the init functions..
			UniversalSearchService::instance()->searchItemsMgr->init();
			UniversalSearchService::instance()->openSearchHandler->scanExistingPlugins();
			UniversalSearchService::instance()->postInit();
		}
		else {
			if(UniversalSearchService::instance()->getLocale() != newLocale) {
				luna_critical(s_logChannel, "Locale Changed from  %s :: to %s - shutting down... ", UniversalSearchService::instance()->getLocale().c_str(), newLocale.c_str());
				//locale changed. wipe out the database and quit!!!.
				UniversalSearchPrefsDb::instance()->purgeDatabase();
			
				exit(0);
			}
		}

	return true;
}

bool UniversalSearchService::cbAppMgrBusStatusNotification(LSHandle* lshandle, LSMessage *message,void *user_data) 
{
	
	LSError lsError;
	LSErrorInit(&lsError);
	
	json_object * root = 0;
	const char* payload = LSMessageGetPayload(message);
	if( !payload )
		return false;
	
	root = json_tokener_parse(payload);
	if ((root == NULL) || (is_error(root))) {
		root = NULL;
		return true;
	}

	json_object * label = json_object_object_get(root,"connected");
	if (label != NULL)
	{
		if (json_object_get_boolean(label) == true) 
		{
			//the application manager is on the bus...make a call to receive list of installed apps.
			 if (LSCall(UniversalSearchService::instance()->m_serviceHandlePrivate,"palm://com.palm.applicationManager/listApps",
			    		"{}",
			    		UniversalSearchService::cbAppMgrAppList,NULL,NULL, &lsError) == false) {
				luna_critical(s_logChannel, "call to applicationmanager/listApps failed");
				LSErrorFree(&lsError);
			} 
		}
	}
	else 
	{
		luna_critical(s_logChannel, "Registration Status message parsing error");
	}
	
	if (root && !is_error(root))
		json_object_put(root);
	
	return true;
}

bool UniversalSearchService::cbAppMgrAppList(LSHandle* lshandle, LSMessage *message,void *user_data) 
{
	LSError lserror;
	LSErrorInit(&lserror);
	json_object* label = NULL; 
	json_object* root = NULL;
	json_object* app = NULL;
	json_object* searchInfo = NULL;
	array_list* apps;
	std::string emailId;
	std::string id;
	std::string icon;
	bool success = true;
	
	const char* payload = LSMessageGetPayload(message);		
	if(!payload) {
		luna_critical(s_logChannel, "Payload is missing");
		success = false;
		goto Done;
	}
	
	root = json_tokener_parse(payload);
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Unable to parse json content");
		success = false;
		goto Done;
	}
	
	label = json_object_object_get(root, "apps");
	if(!label || is_error(label)) {
		luna_critical(s_logChannel, "apps is missing");
		success = false;
		goto Done;
	}
	apps = json_object_get_array(label);
	if(!apps) {
		luna_critical(s_logChannel, "unable to get apps array from the payload");
		success = false;
		goto Done;
	}
	
	for (int i = 0; i < array_list_length(apps); i++) {
		json_object* obj = (json_object*) array_list_get_idx(apps, i);

		//Check appId and Vendor defined in the appInfo. If so, copy those properties into UniversalSearch property.
		label = json_object_object_get(obj, "id");
		if(!label || is_error(label))
			continue;
		id = json_object_get_string(label);
	
		//check if the appInfo object has UniversalSearch property defined.
		app = json_object_object_get(obj, "universalSearch");
		if(!app || is_error(app)) {
			//We need to check whether this app was supporting JustType previously. If yes and exist in the list then remove it.
			if(UniversalSearchService::instance()->searchItemsMgr->isItemExist(id)) {
				UniversalSearchService::instance()->searchItemsMgr->removeSearchItem(json_object_get_string(obj));
				UniversalSearchService::instance()->searchItemsMgr->removeActionProvider(json_object_get_string(obj));
				UniversalSearchService::instance()->searchItemsMgr->removeDBSearchItem(json_object_get_string(obj));
			}
			continue;
		}
		
		//Get the icon from app descriptor
		label = json_object_object_get(obj, "icon");
		if(label && !is_error(label))
			icon = json_object_get_string(label);
		
		//Is Search item exist?
		searchInfo = json_object_object_get(app, "search");
		if(searchInfo && !is_error(searchInfo)) {
			json_object_object_add(searchInfo, "id", json_object_new_string(id.c_str()));
			//For apps, override the value of "url" property to set AppId value.
			json_object_object_add(searchInfo, "url", json_object_new_string(id.c_str()));
			json_object_object_add(searchInfo, "type", json_object_new_string("app"));
			label = json_object_object_get(searchInfo, "iconFilePath");
			if(!label || is_error(label)) {
				json_object_object_add(searchInfo, "iconFilePath", json_object_new_string(icon.c_str()));
			}
			UniversalSearchService::instance()->searchItemsMgr->addSearchItem(json_object_get_string(searchInfo), true, true,true);
		}
		
		//Is Action item exist?
		searchInfo = json_object_object_get(app, "action");
		if(searchInfo && !is_error(searchInfo)) {
			json_object_object_add(searchInfo, "id", json_object_new_string(id.c_str()));
			//For apps, override the value of "url" property to set AppId value.
			json_object_object_add(searchInfo, "url", json_object_new_string(id.c_str()));
			label = json_object_object_get(searchInfo, "iconFilePath");
			if(!label || is_error(label)) {
				json_object_object_add(searchInfo, "iconFilePath", json_object_new_string(icon.c_str()));
			}
			UniversalSearchService::instance()->searchItemsMgr->addActionProvider(json_object_get_string(searchInfo), true, true, true);
		}
		
		//Is MojoDb Search item exist?
		searchInfo = json_object_object_get(app, "dbsearch");
		if(searchInfo && !is_error(searchInfo)) {
			json_object_object_add(searchInfo, "id", json_object_new_string(id.c_str()));
			label = json_object_object_get(searchInfo, "iconFilePath");
			if(!label || is_error(label)) {
				json_object_object_add(searchInfo, "iconFilePath", json_object_new_string(icon.c_str()));
			}
			UniversalSearchService::instance()->searchItemsMgr->addDBSearchItem(json_object_get_string(searchInfo), true, true,true);
		}
	}

	UniversalSearchService::instance()->searchItemsMgr->checkIntegrity();
				
	Done:
	
		if (root && !is_error(root))
			json_object_put(root);
	
		if(!success) {
			return false;
		}

	return true;
	
}

bool UniversalSearchService::cbAppInstallerBusStatusNotification(LSHandle* lshandle, LSMessage *message,void *user_data) 
{
	LSError lsError;
	LSErrorInit(&lsError);
	
	json_object * root = 0;
	const char* payload = LSMessageGetPayload(message);
	if( !payload )
		return false;
	
	root = json_tokener_parse(payload);
	if ((root == NULL) || (is_error(root))) {
		root = NULL;
		return true;
	}

	json_object * label = json_object_object_get(root,"connected");
	if (label != NULL)
	{
		if (json_object_get_boolean(label) == true) 
		{
			//the application installer is on the bus...make a call to receive list of installed apps.
			 if (LSCall(UniversalSearchService::instance()->m_serviceHandlePrivate,"palm://com.palm.appinstaller/notifyOnChange",
			    		"{\"subscribe\":true}",
			    		UniversalSearchService::cbAppInstallerNotifyOnChange,NULL,NULL, &lsError) == false) {
				luna_critical(s_logChannel, "call to applicationmanager/listApps failed");
				LSErrorFree(&lsError);
			} 
		}
	}
	else 
	{
		luna_critical(s_logChannel, "Registration Status message parsing error");
	}
	
	if (root && !is_error(root))
		json_object_put(root);
	
	return true;
	
}

bool UniversalSearchService::cbAppInstallerNotifyOnChange(LSHandle* lshandle, LSMessage *message,void *user_data)
{
	json_object* label = NULL; 
	json_object* root = NULL;
	json_object* params = json_object_new_object();
	bool success = true;
	std::string status;
	std::string appId;

	LSError lserror;
	LSErrorInit(&lserror);
	
	const char* payload = LSMessageGetPayload(message);		
	if(!payload) {
		luna_critical(s_logChannel, "Payload is missing");
		success = false;
		goto Done;
	}
	
	root = json_tokener_parse(payload);
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Unable to parse json content");
		success = false;
		goto Done;
	}
	
	label = json_object_object_get(root, "statusChange");
	if(!label || is_error(label)) {
		success = false;
		goto Done;
	}
	status = json_object_get_string(label);
	
	label = json_object_object_get(root, "appId");
	if(!label || is_error(label)) {
		luna_critical(s_logChannel, "appId is missing!");
		success = false;
		goto Done;
	}
	appId = json_object_get_string(label);
	
	if(status == "INSTALLED") {
		//get the appInfo.
		json_object_object_add(params, "appId", json_object_new_string(appId.c_str()));
		 if (LSCall(UniversalSearchService::instance()->m_serviceHandlePrivate,"palm://com.palm.applicationManager/getAppInfo",
				 json_object_to_json_string (params),
		    		UniversalSearchService::cbAppMgrGetAppInfo,NULL,NULL, &lserror) == false) {
			luna_critical(s_logChannel, "call to applicationmanager/getAppInfo failed");
			LSErrorFree(&lserror);
		} 
	}
	else if(status == "REMOVED") {
		if(UniversalSearchService::instance()->searchItemsMgr->isItemExist(appId)) {
			//App has been removed. Remove the Search entry from all 3 lists.
			json_object_object_add(root, "id", json_object_new_string(appId.c_str()));
			UniversalSearchService::instance()->searchItemsMgr->removeSearchItem(json_object_get_string(root));
			UniversalSearchService::instance()->searchItemsMgr->removeActionProvider(json_object_get_string(root));
			UniversalSearchService::instance()->searchItemsMgr->removeDBSearchItem(json_object_get_string(root));
			
			luna_critical(s_logChannel, "Posting change notificaiton");
			UniversalSearchService::instance()->postSearchListChange("remove");
			
		}
	}
	
	Done:
	
		if (root && !is_error(root))
			json_object_put(root);
		
		if (params && !is_error(params))
			json_object_put(params);
	
		if(!success) {
			return false;
		}

	return true;
	
}

bool UniversalSearchService::cbAppMgrGetAppInfo(LSHandle* lshandle, LSMessage *message,void *user_data)
{
	LSError lserror;
	LSErrorInit(&lserror);
	json_object* label = NULL; 
	json_object* root = NULL;
	json_object* appInfo = NULL;
	json_object* searchInfo = NULL;
	json_object* iconProp = NULL;
	std::string id;
	std::string icon;
	bool success = true;
	
	const char* payload = LSMessageGetPayload(message);		
	if(!payload) {
		luna_critical(s_logChannel, "Payload is missing");
		success = false;
		goto Done;
	}
	
	root = json_tokener_parse(payload);
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Unable to parse json content");
		success = false;
		goto Done;
	}
	
	appInfo = json_object_object_get(root, "appInfo");
	if(!appInfo || is_error(appInfo)) {
		luna_critical(s_logChannel, "appInfo is missing");
		success = false;
		goto Done;
	}

	//Get the AppId.
	label = json_object_object_get(appInfo, "id");
	if(!label || is_error(label)) {
		success = false;
		goto Done;
	}
		
	id = json_object_get_string(label);

	searchInfo = json_object_object_get(appInfo, "universalSearch");
	if(!searchInfo || is_error(searchInfo)) {
		if(UniversalSearchService::instance()->searchItemsMgr->isItemExist(id)) {
			//App has been removed. Remove the Search entry from all 3 lists.
			UniversalSearchService::instance()->searchItemsMgr->removeSearchItem(json_object_get_string(appInfo));
			UniversalSearchService::instance()->searchItemsMgr->removeActionProvider(json_object_get_string(appInfo));
			UniversalSearchService::instance()->searchItemsMgr->removeDBSearchItem(json_object_get_string(appInfo));

			luna_critical(s_logChannel, "Posting change notificaiton");
			UniversalSearchService::instance()->postSearchListChange("remove");
		}
		success = false;
		goto Done;
	}
	
	//Property exist. Check appId and Vendor defined in the appInfo. If so, copy those properties into UniversalSearch property.
	//Get the icon from app descriptor
	label = json_object_object_get(appInfo, "icon");
	if(label && !is_error(label))
		icon = json_object_get_string(label);
	
	//Is Search item exist?
	label = json_object_object_get(searchInfo, "search");
	if(label && !is_error(label)) {
		json_object_object_add(label, "id", json_object_new_string(id.c_str()));
		//For apps, override the value of "url" property to set AppId value.
		json_object_object_add(label, "url", json_object_new_string(id.c_str()));
		json_object_object_add(label, "type", json_object_new_string("app"));
		iconProp = json_object_object_get(label, "iconFilePath");
		if(!iconProp || is_error(iconProp)) {
			json_object_object_add(label, "iconFilePath", json_object_new_string(icon.c_str()));
		}
		success = UniversalSearchService::instance()->searchItemsMgr->addSearchItem(json_object_get_string(label), true, true,true);
	}
	
	//Is Action item exist?
	label = json_object_object_get(searchInfo, "action");
	if(label && !is_error(label)) {
		json_object_object_add(label, "id", json_object_new_string(id.c_str()));
		//For apps, override the value of "url" property to set AppId value.
		json_object_object_add(label, "url", json_object_new_string(id.c_str()));
		iconProp = json_object_object_get(label, "iconFilePath");
		if(!iconProp || is_error(iconProp)) {
			json_object_object_add(label, "iconFilePath", json_object_new_string(icon.c_str()));
		}
		success = UniversalSearchService::instance()->searchItemsMgr->addActionProvider(json_object_get_string(label), true, true, true);
	}
	
	//Is MojoDb Search item exist?
	label = json_object_object_get(searchInfo, "dbsearch");
	if(label && !is_error(label)) {
		json_object_object_add(label, "id", json_object_new_string(id.c_str()));
		iconProp = json_object_object_get(label, "iconFilePath");
		if(!iconProp || is_error(iconProp)) {
			json_object_object_add(label, "iconFilePath", json_object_new_string(icon.c_str()));
		}
		success = UniversalSearchService::instance()->searchItemsMgr->addDBSearchItem(json_object_get_string(label), true, true,true);
	}
	
	Done:
	
		if (root && !is_error(root))
			json_object_put(root);
	
		if(!success) {
			return false;
		}
		else {
			luna_critical(s_logChannel, "Posting change notificaiton");
			UniversalSearchService::instance()->postSearchListChange("new");
		}

	return true;
	
	
}


static bool cbAddOptionalSearchDesc(LSHandle* lshandle, LSMessage *message, void *user_data) 
{
    LSError lserror;
    LSErrorInit(&lserror);
    bool success = false;

    const char* payload = NULL;
    json_object *root = NULL, *label = NULL, *response = NULL;
    std::string errMsg;
    std::string xmlUrl;
    char* uriScheme;

    payload = LSMessageGetPayload (message);
    if (!payload) {
	errMsg = "No payload, ignoring call";
	goto done;
    }

    root = json_tokener_parse (payload);
    if (!root || is_error (root)) {
	errMsg = "Unable to parse payload, ignoring call";
	goto done;
    }

    label = json_object_object_get (root, "xmlUrl");
    if (!label || is_error (label)) {
	errMsg = "No xmlUrl parameter, invalid call";
	goto done;
    }

    xmlUrl = json_object_get_string (label);
    if (xmlUrl.empty()) {
	errMsg = "xmlUrl link does not exist";
	goto done;
    }

	if(!OpenSearchHandler::instance()->checkForDuplication(xmlUrl) && OpenSearchHandler::instance()->getOptionalListSize() == MAXOPENSEARCHES)
	{
		g_debug("Open Search Items limit exceeded");
		errMsg = "Open Search Items limit exceeded";
		goto done;
	}
	
	//Check if it is a valid URL.
	uriScheme = g_uri_parse_scheme(xmlUrl.c_str());
	
	if(uriScheme == NULL) {
		g_debug("Incomplete URL provided");
		errMsg = "Incomplete URL provided";
		goto done;
	}
	
	if(strcmp(uriScheme, "http") != 0)
	{
		g_debug("Invalid URL Provided");
		errMsg = "Invalid URL Provided";
		goto done;
	}
	
    success = OpenSearchHandler::instance()->downloadXml(lshandle, xmlUrl);
    if (!success)
	errMsg = "failed to download the xml file";

done:
   response = json_object_new_object();
   json_object_object_add (response, "returnValue" , json_object_new_boolean (success));
   if (!success)
       json_object_object_add (response, "errorMessage", json_object_new_string (errMsg.c_str()));


   if (!LSMessageReply( lshandle, message, json_object_to_json_string (response), &lserror )) 	{
       LSErrorPrint (&lserror, stderr);
       LSErrorFree(&lserror);
   }

   if (root && !is_error (root))
       json_object_put (root);

   json_object_put (response);

   return true;
}


static bool cbGetOptionalSearchList(LSHandle* lshandle, LSMessage *message, void *user_data)
{
    LSError lserror;
    LSErrorInit(&lserror);
    bool subscribed = false;
    
    if (LSMessageIsSubscription(message)) {		
    		if (!LSSubscriptionAdd(lshandle, "getOptionalSearchList",
    				message, &lserror)) {
    			LSErrorFree(&lserror);
    			subscribed = false;
    		}
    		else 
    			subscribed = true;
    }

    json_object* response = OpenSearchHandler::instance()->getOpenSearchList();
    
    json_object_object_add(response, (char*) "subscribed", json_object_new_boolean(subscribed));
    json_object_object_add (response, "returnValue", json_object_new_boolean (true));

    if (!LSMessageReply( lshandle, message, json_object_to_json_string (response), &lserror )) 	{
	LSErrorPrint (&lserror, stderr);
	LSErrorFree(&lserror);
    }

    json_object_put (response);

    return true;
}

static bool cbClearOptionalSearchList(LSHandle* lshandle, LSMessage *message, void *user_data)
{
    LSError lserror;
    LSErrorInit(&lserror);
    bool postSearchListUpdate = false;

    json_object* response = json_object_new_object();

    postSearchListUpdate = OpenSearchHandler::instance()->clearOpenSearchList();
    json_object_object_add (response, "returnValue", json_object_new_boolean (true));

    if (!LSMessageReply( lshandle, message, json_object_to_json_string (response), &lserror )) 	{
	LSErrorPrint (&lserror, stderr);
	LSErrorFree(&lserror);
    }

    json_object_put (response);
    if(postSearchListUpdate) {
    	luna_critical(s_logChannel, "Posting change notificaiton");
    	UniversalSearchService::instance()->postSearchListChange("remove");
    }

    UniversalSearchService::instance()->postOptionalSearchListChange();
    return true;
}

void UniversalSearchService::postOptionalSearchListChange() 
{
	LSSubscriptionIter *iter=NULL;

	LSError lserror;
	LSHandle * lsHandle;
		
	LSErrorInit(&lserror);
	
	json_object* response = OpenSearchHandler::instance()->getOpenSearchList();
	json_object_object_add (response, "returnValue", json_object_new_boolean (true));
	
	// Find out which handle this subscription needs to go to
	bool retVal = LSSubscriptionAcquire(m_serviceHandlePrivate, "getOptionalSearchList", &iter, &lserror);
	if (retVal) {
		lsHandle = m_serviceHandlePrivate;
		while (LSSubscriptionHasNext(iter)) {
			LSMessage *message = LSSubscriptionNext(iter);
			if (!LSMessageReply(lsHandle,message,json_object_to_json_string (response),&lserror)) {
				LSErrorPrint(&lserror,stderr);
				LSErrorFree(&lserror);
			}
		}
	
		LSSubscriptionRelease(iter);
	}
	else {
		LSErrorFree(&lserror);
	}
	
	json_object_put(response);
}

static bool cbRemoveOptionalSearchItem(LSHandle* lshandle, LSMessage *message, void *user_data)
{
    LSError lserror;
    LSErrorInit(&lserror);

    const char* payload = NULL;
    json_object *root = NULL, *label = NULL, *response = NULL;
    std::string id;
    std::string errMsg;
    bool success = false;

    payload = LSMessageGetPayload (message);
    if (!payload) {
	errMsg = "No payload, ignoring call";
	goto done;
    }

    root = json_tokener_parse (payload);
    if (!root || is_error (root)) {
	errMsg = "Unable to parse payload, ignoring call";
	goto done;
    }

    label = json_object_object_get (root, "id");
    if (!label || is_error (label)) {
	errMsg = "No id parameter, invalid call";
	goto done;
    }

    id = json_object_get_string (label);
    if (id.empty()) {
	errMsg = "Invalid id";
	goto done;
    }
    
    success = OpenSearchHandler::instance()->clearOpenSearchItem (id);
    if (!success)
	errMsg = "Unable to clear item";

done:
    response = json_object_new_object();

    json_object_object_add (response, "returnValue", json_object_new_boolean (success));
    if (!success)
	json_object_object_add (response, "errorMessage", json_object_new_string (errMsg.c_str()));

    if (!LSMessageReply( lshandle, message, json_object_to_json_string (response), &lserror )) 	{
	LSErrorPrint (&lserror, stderr);
	LSErrorFree(&lserror);
    }

    json_object_put (response);
    return true;
}


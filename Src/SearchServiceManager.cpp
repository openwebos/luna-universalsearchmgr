// @@@LICENSE
//
//      Copyright (c) 2010-2013 LG Electronics, Inc.
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

#include "SearchServiceManager.h"
#include "USUtils.h"

#include "Logging.h"

static const char* s_logChannel = "SearchServiceManger";
SearchServiceManager* SearchServiceManager::s_ssmgr_instance = 0;

SearchServiceManager* SearchServiceManager::instance() 
{
	
	if(!s_ssmgr_instance) {
		return new SearchServiceManager();
	}
	
	return s_ssmgr_instance;
}

SearchServiceManager::SearchServiceManager() 
{
	s_ssmgr_instance = this;
}

SearchServiceManager::~SearchServiceManager() 
{
	s_ssmgr_instance = 0;
}

bool SearchServiceManager::addSearchServiceInfo(const char* jsonStr) 
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label;
	bool success = true;
	std::string domainName = "";
	unsigned long timeout = 0L;
	SearchServiceInfo* serviceInfoObj = new SearchServiceInfo();
			
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse content into json");
		success = false;
		goto Done;
	}
		
	label = json_object_object_get(root, "title");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "Title is missing");
		success = false;
		goto Done;
	}
	serviceInfoObj->m_title = json_object_get_string(label);
	
	label = json_object_object_get(root, "serviceURL"); //Format com.XXX.XXX/<category>.../<method>
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "Service URL is missing");
		success = false;
		goto Done;
	}
	serviceInfoObj->m_serviceURL = json_object_get_string(label);
	
	if(USUtils::getServiceDomainPart(serviceInfoObj->m_serviceURL, domainName)) {
		serviceInfoObj->m_serviceID = domainName;
	}
	else {
		luna_critical(s_logChannel, "Unable to parse service url to get domain name!");
		success = false;
		goto Done;
	}
	
	label = json_object_object_get(root, "action");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "Action is missing");
		success = false;
		goto Done;
	}
	serviceInfoObj->m_action = json_object_get_string(label);
	
	label = json_object_object_get(root, "timeout");
	if (!label || is_error(label)) {
		//Default timeout - 5 seconds
		serviceInfoObj->m_timeout = 5000;
	}
	timeout = (unsigned long) json_object_get_int(label);
	//Check the given timeout value
	if(timeout > 1000 && timeout < 20000)
		serviceInfoObj->m_timeout = timeout;
	else
		serviceInfoObj->m_timeout = 5000;
	
	//By default, enabled field is set to false when service is added to the list.
	serviceInfoObj->m_enabled = false;
	
	//All service objects are removable by user.
	serviceInfoObj->m_preventDelete = false;
	
	//All validation passed. Add it to the list
	m_serviceInfoList.push_back(serviceInfoObj);
	
	//Save it to the Database.
 	//dbHandler->syncPrefDb(searchListObj);
			
	Done:
		serviceInfoObj = NULL;
		if(success)
			json_object_put(root);
		else
			return false;
		
	return true;
}

bool SearchServiceManager::removeSearchServiceInfo(const char* jsonStr) 
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label;
	bool success = true;
	std::string domainName = "";
	std::string serviceURL;
				
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse content into json");
		success = false;
		goto Done;
	}
			
	label = json_object_object_get(root, "serviceURL");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "Title is missing");
		success = false;
		goto Done;
	}
	serviceURL = json_object_get_string(label);
	
	USUtils::getServiceDomainPart(serviceURL, domainName);
	
	//Iterate the list to find the matching object.
	for(ServiceInfoList::iterator it=m_serviceInfoList.begin(); it!=m_serviceInfoList.end(); ++it) {
		SearchServiceInfo* serviceInfoObj = (*it);
		if(serviceInfoObj->m_serviceID == domainName) {
			m_serviceInfoList.erase(it);
			delete serviceInfoObj;
			break;
		}
	}
	
	//Save it to the Database.
	// dbHandler->syncPrefDb(searchListObj);
	
	Done:
		if(success)
			json_object_put(root);
		else
			return false;
			
	return true;	
		
}

bool SearchServiceManager::modifySearchServiceInfo(const char* jsonStr) 
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label;
	bool success = true;
	bool enabled;
	std::string serviceURL;
	std::string domainName = "";
						
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse content into json");
		success = false;
		goto Done;
	}
				
	label = json_object_object_get(root, "serviceURL");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "serviceURL is missing");
		success = false;
		goto Done;
	}
	serviceURL = json_object_get_string(label);
		
	USUtils::getServiceDomainPart(serviceURL, domainName);
	
	label = json_object_object_get(root, "enabled");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "enabled is missing");
		success = false;
		goto Done;
	}
	enabled = json_object_get_boolean(label);
		
	//Iterate the list to find the matching object.
	for(ServiceInfoList::iterator it=m_serviceInfoList.begin(); it!=m_serviceInfoList.end(); ++it) {
		SearchServiceInfo* serviceInfoObj =  (*it);
		if(serviceInfoObj->m_serviceID == domainName) {
			serviceInfoObj->m_enabled = enabled;
			break;
		}
	}
		
	//Save it to the Database.
	//dbHandler->syncPrefDb(searchListObj);
		
	Done:
		if(success)
			json_object_put(root);
		else
			return false;
			
	return true;	
	
}

json_object* SearchServiceManager::getSearchServiceList() 
{
	json_object* objArray = json_object_new_array();
	
	for(ServiceInfoList::iterator it=m_serviceInfoList.begin(); it != m_serviceInfoList.end(); ++it) {
		
		const SearchServiceInfo* serviceInfo = (*it);
		
		json_object* infoObj = json_object_new_object();
		json_object_object_add(infoObj,(char*) "serviceID",json_object_new_string((char*) serviceInfo->m_serviceID.c_str()));
		json_object_object_add(infoObj,(char*) "title",json_object_new_string((char*) serviceInfo->m_title.c_str()));
		json_object_object_add(infoObj,(char*) "serviceURL",json_object_new_string((char*) serviceInfo->m_serviceURL.c_str()));
		json_object_object_add(infoObj,(char*) "action",json_object_new_string((char*) serviceInfo->m_action.c_str()));
		json_object_object_add(infoObj,(char*) "timeout",json_object_new_int((int) serviceInfo->m_timeout));
		json_object_object_add(infoObj,(char*) "enabled",json_object_new_boolean(serviceInfo->m_enabled));
		json_object_object_add(infoObj,(char*) "preventDelete",json_object_new_boolean(serviceInfo->m_preventDelete));
		
		json_object_array_add(objArray, infoObj);
	}
	
	return objArray;
	
}

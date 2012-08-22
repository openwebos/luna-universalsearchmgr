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

#ifndef SEARCHSERVICEMANAGER_H_
#define SEARCHSERVICEMANAGER_H_

#include <string>
#include <cjson/json.h>
#include <glib.h>
#include <list>

class SearchServiceInfo {
	
public:
	std::string m_serviceID;
	std::string m_title;
	std::string m_serviceURL;
	std::string m_action;
	unsigned long m_timeout;
	bool m_enabled;
	bool m_preventDelete;
	
	SearchServiceInfo(const std::string& serviceID, const std::string& title, const std::string& serviceURL, const std::string& action, const unsigned long timeout, const bool enabled, const bool preventDelete)
		:m_serviceID(serviceID), m_title(title), m_serviceURL(serviceURL), m_action(action), m_timeout(timeout), m_enabled(enabled), m_preventDelete(preventDelete) {};
	
	SearchServiceInfo() : m_timeout(5000), m_enabled(false), m_preventDelete(false) {};
	
	bool operator==(const SearchServiceInfo& c) {
		//just compare the serviceID and serviceURL
		if(c.m_serviceID != m_serviceID)
			return false;
		if(c.m_serviceURL != m_serviceURL)
			return false;
	
		return true;
	}
	
};

class SearchServiceManager {
	
public:
	static SearchServiceManager* instance();
	
	bool addSearchServiceInfo(const char* jsonStr);
	bool removeSearchServiceInfo(const char* jsonStr);
	bool modifySearchServiceInfo(const char* jsonStr);
	bool isExist(SearchServiceManager* serviceInfo);
	json_object* getSearchServiceList();
	
	
private: 
	static SearchServiceManager* s_ssmgr_instance;
	SearchServiceManager();
	~SearchServiceManager();
	
	typedef std::list<SearchServiceInfo*> ServiceInfoList;
	ServiceInfoList m_serviceInfoList;
	
};


#endif


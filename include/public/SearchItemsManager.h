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

#ifndef __SearchItemsManager_h__
#define __SearchItemsManager_h__

#include <string>
#include <glib.h>
#include <iostream>
#include <cjson/json.h>
#include <list>

#include "UniversalSearchPrefsDb.h"


class SearchItemsManager {
	
public:
	SearchItemsManager();
	~SearchItemsManager();
	static SearchItemsManager* instance();
	
	
	bool validate(const std::string& key, json_object* value);
	bool isValidURL(std::string& url);
	bool isImageFileExist(const std::string& filePath);
	
	void findTarget(const std::string& url);
	void readFromDatabase();
	void readFromDefaultFile();
	void readFromCustFile();
	
	json_object* getSearchList();
	bool addSearchItem(const char* jsonStr, bool dbSync, bool overwrite, bool checkAppExist);
	bool modifySearchItem(const char* jsonStr);
	bool removeSearchItem(const char* jsonStr);
	bool reorderSearchItem(const char* jsonStr);
	bool isSearchItemExist(const std::string& id);
	bool replaceSearchItem(const std::string& id, const std::string& url, const std::string& suggestUrl, const std::string& displayName);
	bool moveSearchItem(const std::string& id, int fromIndex, int toIndex);
	bool modifyAllSearchItems(const char* jsonStr);
	bool removeDisabledOpenSearchItem(const std::string& id);
	
	//Action Providers
	bool addActionProvider(const char* jsonStr, bool dbSync, bool overwrite, bool checkAppExist);
	bool modifyActionProvider(const char* jsonStr);
	bool removeActionProvider(const char* jsonStr);
	bool reorderActionProvider(const char* jsonStr);
	json_object* getActionProvidersList();
	bool moveActionItem(const std::string& id, int fromIndex, int toIndex);
	bool modifyAllActionProviders(const char* jsonStr);
	
	//MojoDb Search Items
	bool addDBSearchItem(const char* jsonStr, bool dbSync, bool overwrite, bool checkAppExist);
	bool modifyDBSearchItem(const char* jsonStr);
	bool removeDBSearchItem(const char* jsonStr);
	bool reorderDBSearchItem(const char* jsonStr);
	json_object* getDBSearchItemList();
	bool moveDBSearchItem(const std::string& id, int fromIndex, int toIndex);
	bool modifyAllDBSearchItems(const char* jsonStr);
	
	bool validateDbSearchItem(std::string appId, const char* dbQuery);
	
	bool isItemExist(const std::string& id);
	
	void init();
	
	void checkIntegrity();

	void dumpList();
	void dumpActionList();

private:
	
	UniversalSearchPrefsDb* dbHandler;
	json_object* searchListObj;
	std::string m_searchPrefStr;
	static SearchItemsManager* s_simgr_instance;
	
	struct ActionProvider {
		std::string id;
		std::string displayName;
		std::string url;
		std::string suggestURL;
		std::string launchParam;
		std::string iconFilePath;
		std::string type;
		bool enabled;
		int version;
		bool appExist;
	};
	
	typedef std::list<ActionProvider> ActionProvidersList;
	ActionProvidersList m_actionProvidersList;
	
	struct SearchProvider {
		std::string id;
		std::string displayName;
		std::string url;
		std::string suggestURL;
		std::string launchParam;
		std::string iconFilePath;
		std::string type;
		bool enabled;
		int version;
		bool appExist;
	};
	
	typedef std::list<SearchProvider> SearchProvidersList;
	SearchProvidersList m_searchProvidersList;
	
	struct MojoDBSearchItem {
		std::string id;
		std::string displayName;
		std::string launchParam;
		std::string launchParamDbField;
		std::string url;
		std::string iconFilePath;
		std::string dbQuery;
		std::string displayFields;
		bool batchQuery;
		bool enabled;
		int version;
		bool appExist;
	};
	
	typedef std::list<MojoDBSearchItem> MojoDBSearchItemList;
	MojoDBSearchItemList m_mojodbSearchItemList;

	void syncPrefDb();
	
	struct PredSearch
	{
	   bool operator()(const SearchProvider& item) const
	   {
	      return item.type == "app" && !item.appExist && item.id != "map";
	   }
	};

	struct PredAction
	{
	   bool operator()(const ActionProvider& item) const
	   {
	      return !item.appExist;
	   }
	};

	struct PredDbSearch
	{
	   bool operator()(const MojoDBSearchItem& item) const
	   {
	      return !item.appExist;
	   }
	};

};

#endif


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

#ifndef __UniversalSearchPrefsDb_h__
#define __UniversalSearchPrefsDb_h__

#include <string>
#include <sqlite3.h>

class UniversalSearchPrefsDb {
public:
	
	static UniversalSearchPrefsDb* instance();
	int readPrefDb(json_object* searchListJsonObj);
	
	bool addSearchRecord(const char* id, const char* category, const char* displayName, const char* iconFilePath, const char* url, const char* suggestURL, const char* launchParam, const char* type, int enabled, int version);
	bool updateSearchRecord(const char* id, const char* category, int enabled);
	bool updateAllSearchRecord(const char* category, int enabled);
	bool removeSearchRecord(const char* id, const char* category);
	bool addDBSearchRecord(const char* id, const char* category, const char* displayName, const char* iconFilePath, const char* url, const char* launchParam, const char* launchParamDbField, const char* dbQuery, const char* displayFields, int batchQuery, int enabled, int version);
	bool updateDBSearchRecord(const char* id, int enabled);
	bool updateAllDBSearchRecord(const char* category, int enabled);
	bool removeDBSearchRecord(const char* id);
	std::string getSearchPreference(const std::string& key);
	bool getAllSearchPreference(json_object* searchPrefObj);
	bool setSearchPreference(const std::string& key, const std::string& val);
	bool syncSearchPreferenceDb(const char* jsonStr);
	
	bool purgeDatabase();

private:
	UniversalSearchPrefsDb();
	~UniversalSearchPrefsDb();
	
	void openUniversalSearchPrefsDb();
	void closeUniversalSearchPrefsDb();
	

private:
	static UniversalSearchPrefsDb* s_uspDb_instance;
	sqlite3* m_uspDb;
	
};

#endif

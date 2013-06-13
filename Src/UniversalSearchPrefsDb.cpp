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

#include <glib.h>
#include <cjson/json.h>
#include <string.h>
#include <iostream>
#include <sstream>

#include "UniversalSearchPrefsDb.h"
#include "Logging.h"

static const char* s_logChannel = "UniversalSearchPrefsDb";
static const char* usp_dbFile = "/var/luna/preferences/universalsearchprefs.db";
UniversalSearchPrefsDb* UniversalSearchPrefsDb::s_uspDb_instance = 0;

UniversalSearchPrefsDb* UniversalSearchPrefsDb::instance()
{
	
	if(!s_uspDb_instance)
		return new UniversalSearchPrefsDb();
	
	return s_uspDb_instance;
}

UniversalSearchPrefsDb::UniversalSearchPrefsDb() 
{
	s_uspDb_instance = this;
	m_uspDb = 0;
	openUniversalSearchPrefsDb();
}

UniversalSearchPrefsDb::~UniversalSearchPrefsDb() 
{
	closeUniversalSearchPrefsDb();
	s_uspDb_instance = 0;
}

void UniversalSearchPrefsDb::openUniversalSearchPrefsDb() 
{
	
	if (m_uspDb) 
	{
		g_warning("Universal Search Prefs db already open");
		return;
	}

	gchar* prefDbDirPath = g_path_get_dirname(usp_dbFile);
	g_mkdir_with_parents(prefDbDirPath, 0755);
	
	int ret = sqlite3_open(usp_dbFile, &m_uspDb);
	if (ret) {
		g_warning("Failed to open Universal Search Prefs db");
		return;
	}
	
	//Creating SearchList table
	ret = sqlite3_exec(m_uspDb,
			"CREATE TABLE IF NOT EXISTS SearchList "
			"(id TEXT, "
			" category TEXT, "
			" displayName TEXT, "
			" iconFilePath TEXT, "
			" url TEXT, "
			" suggestURL TEXT,"
			" launchParam TEXT,"
			" type TEXT, "
			" enabled INTEGER, "
			" version INTEGER, "
			" PRIMARY KEY(id, category) );", NULL, NULL, NULL);
	
	ret = sqlite3_exec(m_uspDb,
			"CREATE TABLE IF NOT EXISTS DBSearchList "
			"(id TEXT PRIMARY KEY, "
			" category TEXT, "
			" displayName TEXT, "
			" iconFilePath TEXT, "
			" url TEXT, "
			" launchParam TEXT,"
			" launchParamDbField TEXT,"
			" dbQuery TEXT,"
			" displayFields TEXT,"
			" batchQuery INTEGER,"
			" enabled INTEGER, "
			" version INTEGER );", NULL, NULL, NULL);
			
	ret = sqlite3_exec(m_uspDb,
			"CREATE TABLE IF NOT EXISTS SearchPreference "
			"(key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE, "
			" value TEXT);", NULL, NULL, NULL);
	
	
	if (ret) {
		g_warning("Failed to create pref table");
		sqlite3_close(m_uspDb);
		m_uspDb = 0;
		return;
	}
	
}

void UniversalSearchPrefsDb::closeUniversalSearchPrefsDb() 
{
	if (!m_uspDb)
		return;

	(void) sqlite3_close(m_uspDb);
	m_uspDb = 0;   
}


bool UniversalSearchPrefsDb::addSearchRecord(const char* id, const char* category, const char* displayName, const char* iconFilePath, const char* url, const char* suggestURL, const char* launchParam, const char* type, int enabled, int version)
{
	if (!m_uspDb) {
		luna_critical(s_logChannel, "Invalid DB handler");
		return false;
	}

	gchar* queryStr = sqlite3_mprintf("INSERT OR REPLACE INTO SearchList "
									  "VALUES (%Q, %Q, %Q, %Q, %Q, %Q, %Q, %Q, %d, %d)",
									  id, category, displayName, iconFilePath, url, suggestURL, launchParam, type, enabled, version);
	if (!queryStr)
		return false;
	
	int ret = sqlite3_exec(m_uspDb, queryStr, NULL, NULL, NULL);

	if (ret) {
		luna_critical (s_logChannel, "Failed to execute query: %s", queryStr);
		sqlite3_free(queryStr);
		return false;
	}

	sqlite3_free(queryStr);

	return true;    
	
}

bool UniversalSearchPrefsDb::updateSearchRecord(const char* id, const char* category, int enabled) 
{
	if (!m_uspDb) {
		luna_critical(s_logChannel, "Invalid DB handler");
		return false;
	}
	
	gchar* queryStr = sqlite3_mprintf("UPDATE OR REPLACE SearchList "
									  "SET ENABLED = %d "
									  "WHERE ID = %Q AND CATEGORY = %Q",
									  enabled, id, category);
	if (!queryStr)
		return false;
	
	int ret = sqlite3_exec(m_uspDb, queryStr, NULL, NULL, NULL);

	if (ret) {
		luna_critical (s_logChannel, "Failed to execute update query: %s", queryStr);
		sqlite3_free(queryStr);
		return false;
	}

	sqlite3_free(queryStr);

	return true;    
}

bool UniversalSearchPrefsDb::updateAllSearchRecord(const char* category, int enabled)
{
	if (!m_uspDb) {
		luna_critical(s_logChannel, "Invalid DB handler");
		return false;
	}

	gchar* queryStr = sqlite3_mprintf("UPDATE OR REPLACE SearchList "
									  "SET ENABLED = %d "
									  "WHERE CATEGORY = %Q",
									  enabled, category);
	if (!queryStr)
		return false;

	int ret = sqlite3_exec(m_uspDb, queryStr, NULL, NULL, NULL);

	if (ret) {
		luna_critical (s_logChannel, "Failed to execute update query: %s", queryStr);
		sqlite3_free(queryStr);
		return false;
	}

	sqlite3_free(queryStr);

	return true;
}

bool UniversalSearchPrefsDb::removeSearchRecord(const char* id, const char* category) 
{
	if (!m_uspDb) {
		luna_critical(s_logChannel, "Invalid DB handler");
		return false;
	}
	
	gchar* queryStr = sqlite3_mprintf("DELETE FROM SearchList "
									  "WHERE ID = %Q AND CATEGORY = %Q",
									  id, category);
	if (!queryStr)
		return false;
	
	int ret = sqlite3_exec(m_uspDb, queryStr, NULL, NULL, NULL);

	if (ret) {
		luna_critical (s_logChannel, "Failed to execute delete query: %s", queryStr);
		sqlite3_free(queryStr);
		return false;
	}

	sqlite3_free(queryStr);

	return true;    
}

bool UniversalSearchPrefsDb::addDBSearchRecord(const char* id, const char* category, const char* displayName, const char* iconFilePath, const char* url, const char* launchParam, const char* launchParamDbField, const char* dbQuery, const char* displayFields, int batchQuery, int enabled, int version)
{
	if (!m_uspDb) {
		luna_critical(s_logChannel, "Invalid DB handler");
		return false;
	}

	gchar* queryStr = sqlite3_mprintf("INSERT OR REPLACE INTO DBSearchList "
									  "VALUES (%Q, %Q, %Q, %Q, %Q, %Q, %Q, %Q, %Q, %d, %d, %d)",
									  id, category, displayName, iconFilePath, url, launchParam, launchParamDbField, dbQuery, displayFields, batchQuery, enabled, version);
	if (!queryStr)
		return false;
	
	int ret = sqlite3_exec(m_uspDb, queryStr, NULL, NULL, NULL);

	if (ret) {
		luna_critical (s_logChannel, "Failed to execute query: %s", queryStr);
		sqlite3_free(queryStr);
		return false;
	}

	sqlite3_free(queryStr);

	return true;    
	
}

bool UniversalSearchPrefsDb::updateDBSearchRecord(const char* id, int enabled) 
{
	if (!m_uspDb) {
		luna_critical(s_logChannel, "Invalid DB handler");
		return false;
	}
	
	gchar* queryStr = sqlite3_mprintf("UPDATE OR REPLACE DBSearchList "
									  "SET ENABLED = %d "
									  "WHERE ID = %Q",
									  enabled, id);
	if (!queryStr)
		return false;
	
	int ret = sqlite3_exec(m_uspDb, queryStr, NULL, NULL, NULL);

	if (ret) {
		luna_critical (s_logChannel, "Failed to execute update query: %s", queryStr);
		sqlite3_free(queryStr);
		return false;
	}

	sqlite3_free(queryStr);

	return true;    
}

bool UniversalSearchPrefsDb::updateAllDBSearchRecord(const char* category, int enabled)
{
	if (!m_uspDb) {
		luna_critical(s_logChannel, "Invalid DB handler");
		return false;
	}

	gchar* queryStr = sqlite3_mprintf("UPDATE OR REPLACE DBSearchList "
									  "SET ENABLED = %d "
									  "WHERE CATEGORY = %Q",
									  enabled, category);
	if (!queryStr)
		return false;

	int ret = sqlite3_exec(m_uspDb, queryStr, NULL, NULL, NULL);

	if (ret) {
		luna_critical (s_logChannel, "Failed to execute update query: %s", queryStr);
		sqlite3_free(queryStr);
		return false;
	}

	sqlite3_free(queryStr);

	return true;
}

bool UniversalSearchPrefsDb::removeDBSearchRecord(const char* id) 
{
	if (!m_uspDb) {
		luna_critical(s_logChannel, "Invalid DB handler");
		return false;
	}
	
	gchar* queryStr = sqlite3_mprintf("DELETE FROM DBSearchList "
									  "WHERE ID = %Q",
									   id);
	if (!queryStr)
		return false;
	
	int ret = sqlite3_exec(m_uspDb, queryStr, NULL, NULL, NULL);

	if (ret) {
		luna_critical (s_logChannel, "Failed to execute delete query: %s", queryStr);
		sqlite3_free(queryStr);
		return false;
	}

	sqlite3_free(queryStr);

	return true;    
}

int UniversalSearchPrefsDb::readPrefDb(json_object* searchListJsonObj) 
{
	sqlite3_stmt* statement = 0;
	const char* tail = 0;
	int ret = 0;
	int numOfRows = 0;
	char* queryStr = 0;
	
	if (!m_uspDb)
		return numOfRows;

	queryStr = (char *) "SELECT * FROM SEARCHLIST";
	
	ret = sqlite3_prepare(m_uspDb, queryStr, -1, &statement, &tail);
	if (ret) {
		luna_critical (s_logChannel, "Failed to prepare sql statement: %s", queryStr);
		goto Done;
	}

	ret = sqlite3_step(statement);
	
	while (ret == SQLITE_ROW) {
		json_object* root = json_object_new_object();
		char* res[8];
		bool enabled;
		int version;
		
		res[0] = (char *)sqlite3_column_text(statement, 0);
		res[1] = (char *)sqlite3_column_text(statement, 1);
		res[2] = (char *)sqlite3_column_text(statement, 2);
		res[3] = (char *)sqlite3_column_text(statement, 3);
		res[4] = (char *)sqlite3_column_text(statement, 4);
		res[5] = (char *)sqlite3_column_text(statement, 5);
		res[6] = (char *)sqlite3_column_text(statement, 6);
		res[7] = (char *)sqlite3_column_text(statement, 7);
		
		json_object_object_add(root, "id", json_object_new_string(res[0]));
		json_object_object_add(root, "category", json_object_new_string(res[1]));
		json_object_object_add(root, "displayName", json_object_new_string(res[2]));
		json_object_object_add(root, "iconFilePath", json_object_new_string(res[3]));
		json_object_object_add(root, "url", json_object_new_string(res[4]));
		json_object_object_add(root, "suggestURL", json_object_new_string(res[5]));
		json_object_object_add(root, "launchParam", json_object_new_string(res[6]));
		json_object_object_add(root, "type", json_object_new_string(res[7]));
		 
		enabled = (((int) sqlite3_column_int(statement, 8)) == 1) ? true : false;
		
		
		json_object_object_add(root, "enabled", json_object_new_boolean(enabled));
		
		version = (int) sqlite3_column_int(statement, 9);
		json_object_object_add(root, "version", json_object_new_int(version));

		json_object_array_add(searchListJsonObj, json_object_get(root));
		numOfRows++;
		ret = sqlite3_step(statement);
	}
	
	queryStr = (char *) "SELECT * FROM DBSEARCHLIST";
	
	ret = sqlite3_prepare(m_uspDb, queryStr, -1, &statement, &tail);
	if (ret) {
		luna_critical (s_logChannel, "Failed to prepare sql statement: %s", queryStr);
		goto Done;
	}

	ret = sqlite3_step(statement);
	
	while (ret == SQLITE_ROW) {
		json_object* root = json_object_new_object();
		char* res[10];
		bool enabled, batchQuery;
		int version;
		
		res[0] = (char *)sqlite3_column_text(statement, 0);
		res[1] = (char *)sqlite3_column_text(statement, 1);
		res[2] = (char *)sqlite3_column_text(statement, 2);
		res[3] = (char *)sqlite3_column_text(statement, 3);
		res[4] = (char *)sqlite3_column_text(statement, 4);
		res[5] = (char *)sqlite3_column_text(statement, 5);
		res[6] = (char *)sqlite3_column_text(statement, 6);
		res[7] = (char *)sqlite3_column_text(statement, 7);
		res[8] = (char *)sqlite3_column_text(statement, 8);
		res[9] = (char *)sqlite3_column_text(statement, 9);
		
		json_object_object_add(root, "id", json_object_new_string(res[0]));
		json_object_object_add(root, "category", json_object_new_string(res[1]));
		json_object_object_add(root, "displayName", json_object_new_string(res[2]));
		json_object_object_add(root, "iconFilePath", json_object_new_string(res[3]));
		json_object_object_add(root, "url", json_object_new_string(res[4]));
		json_object_object_add(root, "launchParam", json_object_new_string(res[5]));
		json_object_object_add(root, "launchParamDbField", json_object_new_string(res[6]));
		json_object_object_add(root, "dbQuery", json_object_new_string(res[7]));
		json_object_object_add(root, "displayFields", json_object_new_string(res[8]));
		
		batchQuery = (((int) sqlite3_column_int(statement, 9)) == 1) ? true : false;
		enabled = (((int) sqlite3_column_int(statement, 10)) == 1) ? true : false;
		
		json_object_object_add(root, "batchQuery", json_object_new_boolean(batchQuery));
		json_object_object_add(root, "enabled", json_object_new_boolean(enabled));
		
		version = (int) sqlite3_column_int(statement, 9);
		json_object_object_add(root, "version", json_object_new_int(version));

		json_object_array_add(searchListJsonObj, json_object_get(root));
		numOfRows++;
		ret = sqlite3_step(statement);
	}
		
	Done:

	if (statement)
		sqlite3_finalize(statement);


	return numOfRows;   
}

std::string UniversalSearchPrefsDb::getSearchPreference(const std::string& key)
{
	sqlite3_stmt* statement = 0;
	const char* tail = 0;
	int ret = 0;
	char * queryStr = 0;
	
	std::string result;

	if (!m_uspDb)
		return result;

	if (key.empty())
		goto Done;
	
	queryStr = sqlite3_mprintf("SELECT value FROM SearchPreference WHERE key=%Q",key.c_str());

	if (!queryStr)
		goto Done;
	
	ret = sqlite3_prepare(m_uspDb, queryStr, -1, &statement, &tail);
	if (ret) {
		luna_critical (s_logChannel, "Failed to prepare sql statement: %s", queryStr);
		goto Done;
	}

	ret = sqlite3_step(statement);
	if (ret == SQLITE_ROW) {
		result = (char *)sqlite3_column_text(statement, 0);
	}

Done:

	if (statement)
		sqlite3_finalize(statement);

	if (queryStr)
		sqlite3_free(queryStr);
	
	return result;    
}

bool UniversalSearchPrefsDb::setSearchPreference(const std::string& key, const std::string& val)
{
	char * queryStr = 0;
	if (!m_uspDb)
		return false;

	if (key.empty())
		return false;
	
	queryStr = sqlite3_mprintf("INSERT INTO SearchPreference "
									  "VALUES (%Q, %Q)",
									  key.c_str(), val.c_str());
	
	if (!queryStr)
		return false;

	int ret = sqlite3_exec(m_uspDb, queryStr, NULL, NULL, NULL);

	if (ret) {
		luna_critical (s_logChannel, "Failed to prepare sql statement: %s", queryStr);
		sqlite3_free(queryStr);
		return false;
	}
	
	sqlite3_free(queryStr);
	return true;    
	
}

bool UniversalSearchPrefsDb::getAllSearchPreference(json_object* searchPrefObj)
{
	
	sqlite3_stmt* statement = 0;
	 json_object* root = json_object_new_object();
	const char* tail = 0;
	int ret = 0;
	char * queryStr = 0;
	bool result = false;

	if (!m_uspDb)
		goto Done;
	
	queryStr = (char *)"SELECT key,value FROM SearchPreference";
	
	ret = sqlite3_prepare(m_uspDb, queryStr, -1, &statement, &tail);
	
	if (ret) {
		luna_critical (s_logChannel, "Failed to prepare sql statement: %s", queryStr);
		goto Done;
	}

	ret = sqlite3_step(statement);

	while (ret == SQLITE_ROW) {
	   
		std::string key;
		std::string val;
		
		key = (char*) sqlite3_column_text(statement, 0);
		val = (char*) sqlite3_column_text(statement, 1);

		if(key == "databaseversion") {
			ret = sqlite3_step(statement);
			continue;
		}
		json_object_object_add(root, key.c_str(), json_object_new_string(val.c_str()));
		ret = sqlite3_step(statement);
	}
	json_object_object_add(searchPrefObj, (char*)"SearchPreference", json_object_get(root));
	result = true;

Done:

	if (statement)
		sqlite3_finalize(statement);
	
	if(root && !is_error(root))
		json_object_put(root);
	
	return result;    
}

bool UniversalSearchPrefsDb::syncSearchPreferenceDb(const char* jsonStr) 
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label = NULL;
	std::string dbVersion;
	int fileVersion;
	std::stringstream fileVerString;
	std::string key;
	std::string keyExist;
	
	//Get the version from Database
	key = "databaseversion";
	dbVersion = getSearchPreference(key);
	if(dbVersion.empty()) {
		dbVersion = "1";
		setSearchPreference("databaseversion", dbVersion);
	}
	
	
	//Get the version from object if exist
	label = json_object_object_get(root, "version");
	if(!label || is_error(label)) {
		fileVersion = 0;
	}
	else 
		fileVersion = json_object_get_int(label);
	
	fileVerString << fileVersion;
	
	if(atoi(dbVersion.c_str()) < fileVersion)
		setSearchPreference("databaseversion", fileVerString.str());	
		
	json_object_object_foreach(root, keyStr, val) {
		
		if(val == NULL || keyStr == NULL)
			continue;
		
		if(strcmp(keyStr, "version") == 0) {
			continue;
		}
		
		keyExist = getSearchPreference(keyStr);
		
		
		if(keyExist.empty() || atoi(dbVersion.c_str()) < fileVersion) {
			setSearchPreference(keyStr, json_object_get_string(val));
		}
		
	}
	
	return true;
}

bool UniversalSearchPrefsDb::purgeDatabase() {
	
	
	int ret = sqlite3_exec(m_uspDb, "DROP TABLE SearchList", NULL, NULL, NULL);
	
	ret = sqlite3_exec(m_uspDb, "DROP TABLE DBSearchList", NULL, NULL, NULL);
	
	ret = sqlite3_exec(m_uspDb, "DROP TABLE SearchPreference", NULL, NULL, NULL);
	
	closeUniversalSearchPrefsDb();
	
	return true;
}



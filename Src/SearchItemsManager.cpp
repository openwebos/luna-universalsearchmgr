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

#include "SearchItemsManager.h"
#include "UniversalSearchPrefsDb.h"
#include "UniversalSearchService.h"
#include "Logging.h"
#include "USUtils.h"

static const char* s_logChannel = "SearchItemsManager";
static const char* s_defaultPrefFile = "/usr/palm/universalsearchmgr/resources/en_us/UniversalSearchList.json";
static const char* s_custUniversalSearchPrefFile = "/usr/lib/luna/customization/UniversalSearchList.json";
SearchItemsManager* SearchItemsManager::s_simgr_instance = 0;

SearchItemsManager::SearchItemsManager() 
{
	s_simgr_instance = this;
	dbHandler = UniversalSearchPrefsDb::instance();
	USUtils::initRandomGenerator();

}

SearchItemsManager::~SearchItemsManager() 
{
	
}

SearchItemsManager* SearchItemsManager::instance() 
{
	
	if(!s_simgr_instance) {
		return new SearchItemsManager();
	}
	
	return s_simgr_instance;
}

void SearchItemsManager::init() 
{
	readFromDatabase();
	readFromDefaultFile();
	readFromCustFile();
	
	//Sync the Database
	syncPrefDb();
}

void SearchItemsManager::readFromDatabase() {
	int prefsInDb = 0;
	json_object* label = NULL;
	json_object* searchListObj = json_object_new_array();
	
	prefsInDb = dbHandler->readPrefDb(searchListObj);
	
	if(prefsInDb > 0) {
		for (int i = 0; i < json_object_array_length(searchListObj); i++) {
			json_object* obj = (json_object*) json_object_array_get_idx(searchListObj, i);
			std::string category;
				
			label = json_object_object_get(obj, "category");
			if (!label || is_error(label))
				continue;
			
				category = json_object_get_string(label);
				
				if(category.compare("search") == 0)
					addSearchItem(json_object_get_string(obj), false, false,false);
				else if(category.compare("action") == 0)
					addActionProvider(json_object_get_string(obj), false, false, false);
				else if(category.compare("dbsearch") == 0)
					addDBSearchItem(json_object_get_string(obj), false, false,false);
				else
					continue;
			}
	}
	
	if(searchListObj && !is_error(searchListObj))
		json_object_put(searchListObj);
}

void SearchItemsManager::readFromDefaultFile() 
{
	// Read the locale file
	char localizedPrefFileName[100];
	
	sprintf(localizedPrefFileName, "/usr/palm/universalsearchmgr/resources/%s/UniversalSearchList.json", UniversalSearchService::instance()->getLocale().c_str());
	luna_critical(s_logChannel, "Reading from file :: %s", localizedPrefFileName);
	char* jsonStr = USUtils::readFile(localizedPrefFileName);
	
	if (!jsonStr) {
		luna_critical(s_logChannel, "Failed to load localized file: [%s]", localizedPrefFileName);
		
		jsonStr = USUtils::readFile(s_defaultPrefFile);
		
		if(!jsonStr) {
			luna_critical(s_logChannel, "Fatal Error - Failed to load default file: [%s]", s_defaultPrefFile);
			return;
		}
	}

	json_object* root = 0;
	json_object* label = 0;
	array_list* searchArray = 0;

	root = json_tokener_parse(jsonStr);
	if (!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse preference file contents into json");
		goto Done;
	}

	label = json_object_object_get(root, "UniversalSearchList");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "Failed to get UniversalSearchList entry from preference file");
		goto Done;
	}

	searchArray = json_object_get_array(label);
	if (!searchArray) {
		luna_critical(s_logChannel, "Failed to get search list array from preference file");
		goto Done;
	}

	for (int i = 0; i < array_list_length(searchArray); i++) {
		json_object* obj = (json_object*) array_list_get_idx(searchArray, i);
		std::string category;
		
		label = json_object_object_get(obj, "category");
		if (!label || is_error(label))
			continue;
		
		category = json_object_get_string(label);
		
		if(category == "search")
			addSearchItem(json_object_get_string(obj), false, false, false);
		else if(category.compare("action") == 0)
			addActionProvider(json_object_get_string(obj), false, false, false);
		else if(category.compare("dbsearch") == 0)
			addDBSearchItem(json_object_get_string(obj), false, false, false);
		else
			continue;
	
	}
	
	//Read search Preference
	label = json_object_object_get(root, "SearchPreference");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "Failed to get SearchPreference entry from preference file");
		goto Done;
	}
	
	m_searchPrefStr = json_object_to_json_string(label);
	
	Done:
		
		if(root && !is_error(root))
			json_object_put(root);
				
		delete [] jsonStr;
	
}

void SearchItemsManager::readFromCustFile()
{
	// Read from the localized customization file
	char localizedCustPrefFileName[100];
	sprintf(localizedCustPrefFileName, "/usr/lib/luna/customization/resources/%s/UniversalSearchList.json", UniversalSearchService::instance()->getLocale().c_str());
	luna_critical(s_logChannel, "Reading from file :: %s", localizedCustPrefFileName);

	char* jsonStr = USUtils::readFile(localizedCustPrefFileName);
	json_object* searchPref = NULL;
	bool fileExist = true;
	
	json_object* root = 0;
	json_object* label = 0;
	array_list* searchArray = 0;
	std::string category, id;
	bool remove = false;
	bool enabledExist = false;
	
	if (!jsonStr) {
		luna_critical(s_logChannel, "Failed to load locale cust files: [%s]", localizedCustPrefFileName);

		//Try the default location
		jsonStr = USUtils::readFile(s_custUniversalSearchPrefFile);

		if(!jsonStr) {
			luna_critical(s_logChannel, "Failed to load cust files: [%s]", s_custUniversalSearchPrefFile);
			fileExist = false;
			goto SyncSearchPref;
		}
	}

	root = json_tokener_parse(jsonStr);
	if (!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse preference file contents into json");
		goto Done;
	}

	label = json_object_object_get(root, "UniversalSearchList");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "Failed to get UniversalSearchList entry from preference file");
		goto Done;
	}

	searchArray = json_object_get_array(label);
	if (!searchArray) {
		luna_critical(s_logChannel, "Failed to get search list array from preference file");
		goto Done;
	}

	for (int i = 0; i < array_list_length(searchArray); i++) {
		json_object* obj = (json_object*) array_list_get_idx(searchArray, i);
		
		label = json_object_object_get(obj, "id");
		if(!label || is_error(label)) {
			luna_critical(s_logChannel, "Id is missing in the Cust File");
			continue;
		}
		id = json_object_get_string(label);
		
		//Check this is to remove the object.
		label = json_object_object_get(obj, "remove");
		if(label && !is_error(label)) {
			remove = json_object_get_boolean(label);
		}
		else
			remove = false;
		
		label = json_object_object_get(obj, "category");
		if (label && !is_error(label))
			category = json_object_get_string(label);

		//Check if "enabled" field exist.
		label = json_object_object_get(obj, "enabled");
		if (label && !is_error(label)) {
			enabledExist = true;
		}
		else
			enabledExist = false;
		
		//Remove the object from the list
		if(remove) {
			if(category.empty()) {
				removeSearchItem(json_object_get_string(obj));
				removeActionProvider(json_object_get_string(obj));
				removeDBSearchItem(json_object_get_string(obj));
			}
			else {
				if(category == "search")
					removeSearchItem(json_object_get_string(obj));
				else if(category.compare("action") == 0)
					removeActionProvider(json_object_get_string(obj));
				else if(category.compare("dbsearch") == 0)
					removeDBSearchItem(json_object_get_string(obj));
			}
		}
		else if(enabledExist) {
			if(category.empty()) {
				modifySearchItem(json_object_get_string(obj));
				modifyActionProvider(json_object_get_string(obj));
				modifyDBSearchItem(json_object_get_string(obj));
			}
			else {
				//This could be either an update or a new item. We don't know at this point, hence calling both modify and add methods which will do the right thing.
				if(category == "search") {
					modifySearchItem(json_object_get_string(obj));
					addSearchItem(json_object_get_string(obj), false, false, false);
				}
				else if(category.compare("action") == 0) {
					modifyActionProvider(json_object_get_string(obj));
					addActionProvider(json_object_get_string(obj), false, false, false);
				}
				else if(category.compare("dbsearch") == 0) {
					modifyDBSearchItem(json_object_get_string(obj));
					addDBSearchItem(json_object_get_string(obj), false, false, false);
				}
			}
		}
	}
	
	SyncSearchPref:
	
		searchPref = json_tokener_parse(m_searchPrefStr.c_str());
		if(!searchPref || is_error(searchPref)) {
			luna_critical(s_logChannel, "Failed to parse SearchPreference entry from cust preference file");
			goto Done;
		}
		
		if(fileExist) {
			//Read search Preference
			label = json_object_object_get(root, "SearchPreference");
			if (label && !is_error(label)) {
				json_object_object_foreach(label, key, val) {
		
					if(val == NULL)
						continue;
					
					json_object_object_add(searchPref, key, val);
				}
			}
		}
	
		dbHandler->syncSearchPreferenceDb(json_object_get_string(searchPref));
	
	Done:
				
		delete [] jsonStr;
}

/*
 * Returns the search list array.
 */
json_object* SearchItemsManager::getSearchList() 
{
	json_object* objArray = json_object_new_array();
	
	for(SearchProvidersList::const_iterator it=m_searchProvidersList.begin(); it!=m_searchProvidersList.end(); ++it) {
		
		SearchProvider searchProvider =  (*it);
		
		json_object* infoObj = json_object_new_object();
		json_object_object_add(infoObj,(char*) "id",json_object_new_string((char*) searchProvider.id.c_str()));
		json_object_object_add(infoObj,(char*) "displayName",json_object_new_string((char*) searchProvider.displayName.c_str()));
		json_object_object_add(infoObj,(char*) "iconFilePath",json_object_new_string((char*) searchProvider.iconFilePath.c_str()));
		json_object_object_add(infoObj,(char*) "url",json_object_new_string((char*) searchProvider.url.c_str()));
		json_object_object_add(infoObj,(char*) "suggestURL",json_object_new_string((char*) searchProvider.suggestURL.c_str()));
		json_object_object_add(infoObj,(char*) "launchParam",json_object_new_string((char*) searchProvider.launchParam.c_str()));
		json_object_object_add(infoObj,(char*) "type",json_object_new_string((char*) searchProvider.type.c_str()));
		json_object_object_add(infoObj,(char*) "enabled",json_object_new_boolean(searchProvider.enabled));
		
		json_object_array_add(objArray, infoObj);
		
	}
	return objArray;
}

//add an item to the list
bool SearchItemsManager::addSearchItem(const char* jsonStr, bool dbSync, bool overwrite, bool appExist)
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label = NULL;
	std::string imageFilePath;
	std::string id;
	int version = 1;
	bool enabled = false;
	char idValue[20];
	bool success = true;
	bool imgFileExist = true;
	SearchProvider searchProvider;
	SearchProvidersList::iterator it;
	int itemIndex;
	bool setDefault = false;
	bool replaceItem = false;
	
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse content into json");
		success = false;
		goto Done;
	}

	label = json_object_object_get(root, "id");
	if (!label || is_error(label)) {
		//Get the Unique value
		sprintf(idValue, "User-%d", USUtils::getUniqueId() );
		id = idValue;
	}
	else 
		id = json_object_get_string(label);
	
	label = json_object_object_get(root, "version");
	if (label && !is_error(label)) {
		version = json_object_get_int(label);
	}
	
	if(id.find("com.palm.app",0) != std::string::npos) {
		enabled = true; //Privileged App. Enabled by default.
	}
	
	label = json_object_object_get(root, "enabled");
	if (label && !is_error(label)) {
		enabled = json_object_get_int(label);
	}

	//check for duplication
	//Iterate the list to find the matching object.
	for(it=m_searchProvidersList.begin(), itemIndex=0; it!=m_searchProvidersList.end(); ++it,++itemIndex) {
		SearchProvider searchItem =  (*it);
		if(searchItem.id == id) {
			//Check the flag overwrite is set to true. If truthy then simply replace the entry. but restore the user preference(enable /disable)
			if(overwrite) {
				enabled = searchItem.enabled;
				m_searchProvidersList.erase(it);
				replaceItem = true;
				break;
			}
			//Check the version. Overwrite if it is greater than what is in the Database.
			else if(version > searchItem.version) {
				removeSearchItem(jsonStr);
				break;
			}	
			else {
				luna_critical(s_logChannel, "Search Item already exist");
				success = false;
				goto Done;
			}
		}
	}

	searchProvider.id = id;
	searchProvider.version = version;
	searchProvider.enabled = enabled;
	searchProvider.appExist = appExist;

	label = json_object_object_get(root, "iconFilePath");
	if (label && !is_error(label)) {
		imageFilePath = json_object_get_string(label);
						
		if(!imageFilePath.empty() && USUtils::doesExistOnFilesystem(imageFilePath.c_str())) {
			searchProvider.iconFilePath = imageFilePath;
			imgFileExist = true;
		}
	}

	label = json_object_object_get(root, "displayName");
	if ((!label || is_error(label)) && !imgFileExist) {
		luna_critical(s_logChannel, "Both ImageFile and DisplayName are missing");
		success = false;
		goto Done;
	}
	searchProvider.displayName = json_object_get_string(label);
					
	label = json_object_object_get(root, "url");
	if (!label || is_error(label)) {
		success = false;
		goto Done;
	}
	searchProvider.url = json_object_get_string(label);
	
	label = json_object_object_get(root, "launchParam");
	if (label && !is_error(label)) {
		searchProvider.launchParam = json_object_get_string(label);
	}
	
	
	label = json_object_object_get(root, "type");
	if(!label || is_error(label)) {
		searchProvider.type = "web";
	}
	else 
		searchProvider.type = json_object_get_string(label);
	
	if(searchProvider.type == "app" && searchProvider.launchParam.empty()) {
		//If launch param is empty then there is no point adding this app into Universal Search. 
		success = false;
		goto Done;
	}
	
	label = json_object_object_get(root, "suggestURL");
	if(label && !is_error(label)) {
		searchProvider.suggestURL = json_object_get_string(label);
	}
	
	//Do we need to set it as a default?
	label = json_object_object_get(root, "setDefault");
	if(label && !is_error(label)) {
		setDefault = json_object_get_boolean(label);
	}
			
	//It passes the validation, add it to the list.
	if(replaceItem && overwrite) {
		it=m_searchProvidersList.begin();
		std::advance(it, itemIndex);
		m_searchProvidersList.insert(it, searchProvider);
	}
	else
		m_searchProvidersList.push_back(searchProvider);

	//Save it to the Database.
	if(dbSync)
		dbHandler->addSearchRecord(searchProvider.id.c_str(), "search", searchProvider.displayName.c_str(), searchProvider.iconFilePath.c_str(), searchProvider.url.c_str(), 
				searchProvider.suggestURL.c_str(), searchProvider.launchParam.c_str(), searchProvider.type.c_str(), searchProvider.enabled?1:0, searchProvider.version);
	
	
	if(setDefault) {
		dbHandler->setSearchPreference("defaultSearchEngine", searchProvider.id);
	}
	
	Done:
	
		if (root && !is_error(root))
			json_object_put(root);
	
		if(!success)
			return false;
	
	return true;
}

bool SearchItemsManager::modifySearchItem(const char* jsonStr) 
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label = NULL;
	std::string Id;
	bool success = true;
	bool enabled;
	int index = 0;
	bool setDefault = false;
							
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse content into json");
		success = false;
		goto Done;
	}
				
	label = json_object_object_get(root, "id");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "id is missing");
		success = false;
		goto Done;
	}
	Id = json_object_get_string(label);
			
	label = json_object_object_get(root, "enabled");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "enabled is missing");
		success = false;
		goto Done;
	}
	enabled = json_object_get_boolean(label);
	
	//Do we need to set it as a default?
	label = json_object_object_get(root, "setDefault");
	if(label && !is_error(label)) {
		setDefault = json_object_get_boolean(label);
	}

		
	//Iterate the list to find the matching object.
	for(SearchProvidersList::iterator it=m_searchProvidersList.begin(); it!=m_searchProvidersList.end(); ++it, index++) {
		SearchProvider& searchProvider =  (*it);
		if(searchProvider.id == Id) {
			searchProvider.enabled = enabled;
			dbHandler->updateSearchRecord(Id.c_str(), "search", enabled?1:0);
			/*if(!enabled) {
				moveSearchItem(Id, index, (int)m_searchProvidersList.size());
			}*/
			if(setDefault) {
				dbHandler->setSearchPreference("defaultSearchEngine", Id);
			}
			break;
		}
	}
	
	Done:
		
		if (root && !is_error(root))
			json_object_put(root);

		if(!success)
			return false;

	return true;
}

bool SearchItemsManager::modifyAllSearchItems(const char* jsonStr)
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label = NULL;
	json_object* openSearchList;
	json_object* openSearchObj = json_object_new_array();
	std::string iconFile;
	bool success = true;
	bool enabled;

	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse content into json");
		success = false;
		goto Done;
	}

	label = json_object_object_get(root, "enabled");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "enabled is missing");
		success = false;
		goto Done;
	}
	enabled = json_object_get_boolean(label);


	//Iterate the list to update all objects.
	for(SearchProvidersList::iterator it=m_searchProvidersList.begin(); it!=m_searchProvidersList.end(); ++it) {
		SearchProvider& searchProvider =  (*it);
		searchProvider.enabled = enabled;
	}

	//Check to see if there are items in the opensearch list. if there are, add them to search items list if the request is enabled:true.
	if(enabled) {
		openSearchList = OpenSearchHandler::instance()->getOpenSearchList();

		openSearchObj = json_object_object_get(openSearchList, "Options");

		if (!openSearchObj || is_error(openSearchObj)) {
			luna_critical(s_logChannel, "Options is missing");
			success = false;
			goto Done;
		}
		luna_critical(s_logChannel, "Options length %d", json_object_array_length(openSearchObj));
		for (int i = 0; i < json_object_array_length(openSearchObj); i++) {
			json_object* obj = (json_object*) json_object_array_get_idx(openSearchObj, i);

			label = json_object_object_get(obj, "imageData");
			if (label && !is_error(label)) {
				iconFile = json_object_get_string(label);
			}
			else {
				iconFile = std::string("");
			}

			label = json_object_object_get(obj, "searchUrl");
			if (label && !is_error(label)) {
				json_object_object_add (obj, "url", label);
			}

			label = json_object_object_get(obj, "suggestionUrl");
			if (label && !is_error(label)) {
				json_object_object_add (obj, "suggestURL", label);
			}

			json_object_object_add (obj, "category", json_object_new_string ("search"));
			json_object_object_add (obj, "type", json_object_new_string ("opensearch"));
			json_object_object_add (obj, "enabled", json_object_new_boolean (enabled));
			json_object_object_add (obj, "iconFilePath", json_object_new_string (iconFile.c_str()));
			addSearchItem(json_object_get_string(obj), true, false, true);
		}

	}


	dbHandler->updateAllSearchRecord("search", enabled?1:0);

	Done:

		if (root && !is_error(root))
			json_object_put(root);

		if(openSearchObj && !is_error(openSearchObj))
			json_object_put(openSearchObj);


		if(!success)
			return false;

	return true;
}

bool SearchItemsManager::removeSearchItem(const char* jsonStr) 
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label = NULL;
	bool success = true;
	std::string value;
	std::string id;
			
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse content into json");
		success = false;
		goto Done;
	}
		
	label = json_object_object_get(root, "id");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "ID is missing");
		success = false;
		goto Done;
	}
	id = json_object_get_string(label);
	
	//Iterate the list to find the matching object.
	for(SearchProvidersList::iterator it=m_searchProvidersList.begin(); it!=m_searchProvidersList.end(); ++it) {
		SearchProvider& searchProvider =  (*it);
		if(searchProvider.id == id) {
			m_searchProvidersList.erase(it);
			dbHandler->removeSearchRecord(id.c_str(), "search");
			break;
		}
	}
				
	Done:

		if (root && !is_error(root))
			json_object_put(root);

		if(!success)
			return false;

	return true;
}

bool SearchItemsManager::reorderSearchItem(const char* jsonStr) 
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label = NULL;
	bool success = true;

	std::string id;
	int fromIndex = 0, toIndex = 0;
			
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse content into json");
		success = false;
		goto Done;
	}
		
	label = json_object_object_get(root, "id");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "ID is missing");
		success = false;
		goto Done;
	}
	id = json_object_get_string(label);
	
	/*label = json_object_object_get(root, "fromIndex");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "From Index is missing");
		success = false;
		goto Done;
	}
	fromIndex = json_object_get_int(label);*/

	//Iterate the list to find the matching object.
	for(SearchProvidersList::iterator it=m_searchProvidersList.begin(); it!=m_searchProvidersList.end(); ++it,++fromIndex) {
		SearchProvider& searchProvider =  (*it);
		if(searchProvider.id == id) {
			break;
		}
	}
	
	if(fromIndex < 0 || fromIndex >= (int)m_searchProvidersList.size()) {
		luna_critical(s_logChannel, "fromIndex is out of range");
		success = false;
		goto Done;
	}
	
	label = json_object_object_get(root, "toIndex");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "To Index is missing");
		success = false;
		goto Done;
	}
	toIndex = json_object_get_int(label);
	//The list is divided into two groups in the UI (default and More searches). When reorder occurs  in the More Searches group,
	//we need to increment the toIndex by 1 to include the default item.
	toIndex++;
	
	//If an item moves downward, adjust the toIndex incrementing by 1.
	if(fromIndex < toIndex) {
		toIndex++;
	}

	success = moveSearchItem(id, fromIndex, toIndex);

	Done:

		if (root && !is_error(root))
			json_object_put(root);

		if(!success)
			return false;

	return true;
	
}

bool SearchItemsManager::moveSearchItem(const std::string& id, int fromIndex, int toIndex)
{
	SearchProvidersList::iterator fromIt, toIt;
	
	//Make sure toIndex is within the range.
	if(toIndex > (int)m_searchProvidersList.size())
		toIndex = (int)m_searchProvidersList.size();
	
	fromIt = m_searchProvidersList.begin();
	std::advance(fromIt, fromIndex);
	
	//Make sure it points the item that we are moving.
	if(fromIt->id != id) {
		return false;
	}
	
	toIt = m_searchProvidersList.begin();
	std::advance(toIt, toIndex);
	
	m_searchProvidersList.splice(toIt, m_searchProvidersList, fromIt);


	//Sync the Database
	syncPrefDb();
	
	return true;
}

bool SearchItemsManager::replaceSearchItem(const std::string& id, const std::string& url, const std::string& suggestUrl, const std::string& displayName)
{
	
	//Iterate thru the list to find the item within the list
	for(SearchProvidersList::iterator it=m_searchProvidersList.begin(); it!=m_searchProvidersList.end(); ++it) {
		SearchProvider& searchItem =  (*it);
		if(searchItem.id == id) {
			searchItem.displayName = displayName;
			searchItem.url = url;
			searchItem.suggestURL = suggestUrl;
			break;
		}
	}

	//Sync the Database
	syncPrefDb();
	return true;
}

bool SearchItemsManager::isSearchItemExist(const std::string& id)
{
	for(SearchProvidersList::iterator it=m_searchProvidersList.begin(); it!=m_searchProvidersList.end(); ++it) {
		SearchProvider searchItem =  (*it);
		if(searchItem.id == id) {
			return true;
		}
	}
	return false;
}

bool SearchItemsManager::removeDisabledOpenSearchItem(const std::string& id)
{
	for(SearchProvidersList::iterator it=m_searchProvidersList.begin(); it!=m_searchProvidersList.end(); ++it) {
		SearchProvider& searchProvider =  (*it);
		if(searchProvider.id == id && searchProvider.type == "opensearch" && !searchProvider.enabled) {
			m_searchProvidersList.erase(it);
			dbHandler->removeSearchRecord(id.c_str(), "search");
			return true;
		}
	}
	return false;
}

bool SearchItemsManager::addActionProvider(const char* jsonStr, bool dbSync, bool overwrite, bool appExist)
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label = NULL;
	int version = 1;
	std::string imageFilePath;
	std::string id;
	bool enabled = false;
	bool success = true;
	bool imgFileExist = false;
	bool replaceItem = false;
	int itemIndex;
	ActionProvider actionProvider;
	ActionProvidersList::iterator it;
	
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse content into json");
		success = false;
		goto Done;
	}
	
	label = json_object_object_get(root, "id");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "id is missing");
		success = false;
		goto Done;
	}
	id = json_object_get_string(label);

	label = json_object_object_get(root, "version");
	if (label && !is_error(label)) {
		version = json_object_get_int(label);
	}

	if(id.find("com.palm.app",0) != std::string::npos) {
		enabled = true; //Privileged App. Enabled by default.
	}
	
	label = json_object_object_get(root, "enabled");
	if (label && !is_error(label)) {
		enabled = json_object_get_boolean(label);
	}

	//check for duplication
	//Iterate the list to find the matching object.
	for(it=m_actionProvidersList.begin(), itemIndex=0; it!=m_actionProvidersList.end(); ++it, ++itemIndex) {
		ActionProvider actionInfo =  (*it);
		if(actionInfo.id == id) {
			if(overwrite) {
				enabled = actionInfo.enabled;
				m_actionProvidersList.erase(it);
				replaceItem = true;
				break;
			}
			//Check the version. Overwrite if it is greater than what is in the Database.
			if(version > actionInfo.version) {
				//remove the item from list
				removeActionProvider(jsonStr);
				break;
			}
			else {
				luna_critical(s_logChannel, "Action Item already exist");
				success = false;
				goto Done;
			}
		}
	}
	actionProvider.id = id;
	actionProvider.version = version;
	actionProvider.enabled = enabled;
	actionProvider.appExist = appExist;
	
	label = json_object_object_get(root, "iconFilePath");
	if (label && !is_error(label)) {
		imageFilePath = json_object_get_string(label);
						
		if(!imageFilePath.empty() && USUtils::doesExistOnFilesystem(imageFilePath.c_str())) {
			actionProvider.iconFilePath = imageFilePath;
			imgFileExist = true;
		}
		
	}
	label = json_object_object_get(root, "displayName");
	if ((!label || is_error(label)) && !imgFileExist) {
		luna_critical(s_logChannel, "Both ImageFile and DisplayName are missing");
		success = false;
		goto Done;
	}
	actionProvider.displayName = json_object_get_string(label);			
	label = json_object_object_get(root, "url");
	if (!label || is_error(label)) {
		success = false;
		goto Done;
	}
	actionProvider.url = json_object_get_string(label);
	label = json_object_object_get(root, "launchParam");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "launchParam is missing");
		success = false;
		goto Done;
	}
	
	actionProvider.launchParam = json_object_get_string(label);	
	if(actionProvider.launchParam.empty()) {
		//launch param is empty. ignore the entry.
		success = false;
		goto Done;
	}
	//It passes the validation, add it to the list.
	if(overwrite && replaceItem) {
		it=m_actionProvidersList.begin();
		std::advance(it, itemIndex);
		m_actionProvidersList.insert(it, actionProvider);
	}
	else 
		m_actionProvidersList.push_back(actionProvider);

	//Save it to the Database.
	if(dbSync)
		dbHandler->addSearchRecord(actionProvider.id.c_str(), "action", actionProvider.displayName.c_str(), actionProvider.iconFilePath.c_str(), actionProvider.url.c_str(), 
				actionProvider.suggestURL.c_str(),actionProvider.launchParam.c_str(),actionProvider.type.c_str(),actionProvider.enabled?1:0, actionProvider.version);
	
	Done:

		if (root && !is_error(root))
			json_object_put(root);

		if(!success)
			return false;

	return true;
	
}

bool SearchItemsManager::modifyActionProvider(const char* jsonStr)
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label = NULL;
	std::string id;
	bool success = true;
	bool enabled;
	int index = 0;
						
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse content into json");
		success = false;
		goto Done;
	}
				
	label = json_object_object_get(root, "id");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "Id is missing");
		success = false;
		goto Done;
	}
	id = json_object_get_string(label);
			
	label = json_object_object_get(root, "enabled");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "enabled is missing");
		success = false;
		goto Done;
	}
	enabled = json_object_get_boolean(label);
		
	//Iterate the list to find the matching object.
	for(ActionProvidersList::iterator it=m_actionProvidersList.begin(); it!=m_actionProvidersList.end(); ++it, index++) {
		ActionProvider& actionProvider =  (*it);
		if(actionProvider.id == id) {
			actionProvider.enabled = enabled;
			dbHandler->updateSearchRecord(id.c_str(), "action", enabled?1:0);
			/*if(!enabled) {
				moveActionItem(id, index, (int)m_actionProvidersList.size());
			}*/
			break;
		}
	}
		
	Done:

		if (root && !is_error(root))
			json_object_put(root);

		if(!success)
			return false;

	return true;	
}

bool SearchItemsManager::modifyAllActionProviders(const char* jsonStr)
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label = NULL;
	bool success = true;
	bool enabled;

	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse content into json");
		success = false;
		goto Done;
	}

	label = json_object_object_get(root, "enabled");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "enabled is missing");
		success = false;
		goto Done;
	}
	enabled = json_object_get_boolean(label);

	//Iterate the list to find the matching object.
	for(ActionProvidersList::iterator it=m_actionProvidersList.begin(); it!=m_actionProvidersList.end(); ++it) {
		ActionProvider& actionProvider =  (*it);
		actionProvider.enabled = enabled;
	}
	dbHandler->updateAllSearchRecord("action", enabled?1:0);

	Done:

		if (root && !is_error(root))
			json_object_put(root);

		if(!success)
			return false;

	return true;
}

bool SearchItemsManager::removeActionProvider(const char* jsonStr)
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label = NULL;
	std::string id;
	bool success = true;
						
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse content into json");
		success = false;
		goto Done;
	}
				
	label = json_object_object_get(root, "id");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "Id is missing");
		success = false;
		goto Done;
	}
	id = json_object_get_string(label);
				
	//Iterate the list to find the matching object.
	for(ActionProvidersList::iterator it=m_actionProvidersList.begin(); it!=m_actionProvidersList.end(); ++it) {
		const ActionProvider& actionProvider =  (*it);
		if(actionProvider.id == id) {
			m_actionProvidersList.erase(it);
			dbHandler->removeSearchRecord(id.c_str(), "action");
			break;
		}
	}

	Done:

		if (root && !is_error(root))
			json_object_put(root);

		if(!success)
			return false;
			
	return true;	
}

bool SearchItemsManager::reorderActionProvider(const char* jsonStr)
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label = NULL;
	bool success = true;

	std::string id;
	int fromIndex, toIndex;
			
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse content into json");
		success = false;
		goto Done;
	}
		
	label = json_object_object_get(root, "id");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "ID is missing");
		success = false;
		goto Done;
	}
	id = json_object_get_string(label);
	
	label = json_object_object_get(root, "fromIndex");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "From Index is missing");
		success = false;
		goto Done;
	}
	fromIndex = json_object_get_int(label);
	
	if(fromIndex < 0 || fromIndex >= (int)m_actionProvidersList.size()) {
		luna_critical(s_logChannel, "fromIndex is out of range");
		success = false;
		goto Done;
	}
	
	label = json_object_object_get(root, "toIndex");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "To Index is missing");
		success = false;
		goto Done;
	}
	toIndex = json_object_get_int(label);
	
	//If an item moves downward, adjust the toIndex incrementing by 1.
	if(fromIndex < toIndex) {
		toIndex++;
	}
	
	success = moveActionItem(id, fromIndex, toIndex);
	
	Done:

		if (root && !is_error(root))
			json_object_put(root);

		if(!success)
			return false;

	return true;
	
}

bool SearchItemsManager::moveActionItem(const std::string& id, int fromIndex, int toIndex)
{
	ActionProvidersList::iterator fromIt, toIt;
	
	//Make sure toIndex is within the range.
	if(toIndex > (int)m_actionProvidersList.size())
		toIndex = (int)m_actionProvidersList.size();
	
	fromIt = m_actionProvidersList.begin();
	std::advance(fromIt, fromIndex);
	
	//Make sure it points the item that we are moving.
	if(fromIt->id != id) {
		return false;
	}
	
	toIt = m_actionProvidersList.begin();
	std::advance(toIt, toIndex);
	
	m_actionProvidersList.splice(toIt, m_actionProvidersList, fromIt);

	//Sync the Database
	syncPrefDb();
	
	return true;
}

json_object* SearchItemsManager::getActionProvidersList() 
{
	json_object* objArray = json_object_new_array();
	
	for(ActionProvidersList::const_iterator it=m_actionProvidersList.begin(); it!=m_actionProvidersList.end(); ++it) {
		
		ActionProvider actionProvider =  (*it);
		
		json_object* infoObj = json_object_new_object();
		json_object_object_add(infoObj,(char*) "id",json_object_new_string((char*) actionProvider.id.c_str()));
		json_object_object_add(infoObj,(char*) "displayName",json_object_new_string((char*) actionProvider.displayName.c_str()));
		json_object_object_add(infoObj,(char*) "iconFilePath",json_object_new_string((char*) actionProvider.iconFilePath.c_str()));
		json_object_object_add(infoObj,(char*) "url",json_object_new_string((char*) actionProvider.url.c_str()));
		json_object_object_add(infoObj,(char*) "launchParam",json_object_new_string((char*) actionProvider.launchParam.c_str()));
		json_object_object_add(infoObj,(char*) "enabled",json_object_new_boolean(actionProvider.enabled));
		
		json_object_array_add(objArray, infoObj);
		
	}
	return objArray;
}

void SearchItemsManager::syncPrefDb()
{
	for(SearchProvidersList::const_iterator it=m_searchProvidersList.begin(); it!=m_searchProvidersList.end(); ++it) {
			SearchProvider searchProvider =  (*it);
			dbHandler->addSearchRecord(searchProvider.id.c_str(), "search", searchProvider.displayName.c_str(), searchProvider.iconFilePath.c_str(), searchProvider.url.c_str(), 
					searchProvider.suggestURL.c_str(), searchProvider.launchParam.c_str(), searchProvider.type.c_str(), searchProvider.enabled?1:0, searchProvider.version);
	}

	for(ActionProvidersList::const_iterator it=m_actionProvidersList.begin(); it!=m_actionProvidersList.end(); ++it) {
			ActionProvider actionProvider =  (*it);
			dbHandler->addSearchRecord(actionProvider.id.c_str(), "action", actionProvider.displayName.c_str(), actionProvider.iconFilePath.c_str(), actionProvider.url.c_str(), 
					actionProvider.suggestURL.c_str(),actionProvider.launchParam.c_str(), actionProvider.type.c_str(), actionProvider.enabled?1:0, actionProvider.version);
	}

	for(MojoDBSearchItemList::const_iterator it=m_mojodbSearchItemList.begin(); it!=m_mojodbSearchItemList.end(); ++it) {
			MojoDBSearchItem dbSearch =  (*it);
			dbHandler->addDBSearchRecord(dbSearch.id.c_str(), "dbsearch", dbSearch.displayName.c_str(), dbSearch.iconFilePath.c_str(), dbSearch.url.c_str(), 
					dbSearch.launchParam.c_str(),dbSearch.launchParamDbField.c_str(), dbSearch.dbQuery.c_str(), dbSearch.displayFields.c_str(), dbSearch.batchQuery?1:0,dbSearch.enabled?1:0, dbSearch.version);
	}		
}

bool SearchItemsManager::addDBSearchItem(const char* jsonStr, bool dbSync, bool overwrite, bool appExist)
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label = NULL;
	std::string imageFilePath;
	std::string id;
	int version = 1;
	int itemIndex;
	bool enabled = false;
	bool success = true;
	bool replaceItem = false;
	
	MojoDBSearchItem dbSearchItem;
	MojoDBSearchItemList::iterator it;
	
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse content into json");
		success = false;
		goto Done;
	}

	label = json_object_object_get(root, "id");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "Id is missing");
		success = false;
		goto Done;
	}
	id = json_object_get_string(label);
	
	label = json_object_object_get(root, "version");
	if (label && !is_error(label)) {
		version = json_object_get_int(label);
	}
	
	if(id.find("com.palm.app",0) != std::string::npos) {
		enabled = true; //Privileged App. Enabled by default.
	}
	
	label = json_object_object_get(root, "enabled");
	if (label && !is_error(label)) {
		enabled = json_object_get_boolean(label);
	}

	//check for duplication
	
	//Iterate the list to find the matching object.
	for(it=m_mojodbSearchItemList.begin(), itemIndex=0; it!=m_mojodbSearchItemList.end(); ++it, ++itemIndex) {
		MojoDBSearchItem dbInfo =  (*it);
		if(dbInfo.id == id) {
			if(overwrite) {
				enabled = dbInfo.enabled;
				m_mojodbSearchItemList.erase(it);
				replaceItem = true;
				break;
			}
			//Check the version. Overwrite if it is greater than what is in the Database.
			if(version > dbInfo.version) {
				//remove the item from list
				removeDBSearchItem(jsonStr);
				break;
			}
			else {
				luna_critical(s_logChannel, "DB Search Item already exist");
				success = false;
				goto Done;
			}
		}
	}
	dbSearchItem.id = id;
	dbSearchItem.version = version;
	dbSearchItem.enabled = enabled;
	dbSearchItem.appExist = appExist;

	label = json_object_object_get(root, "dbQuery");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "DbQuery property is missing");
		success = false;
		goto Done;
	}
	
	dbSearchItem.dbQuery = json_object_get_string(label);
	if(!validateDbSearchItem(id, dbSearchItem.dbQuery.c_str())) {
		luna_critical(s_logChannel, "DbQuery Validation Failed");
		success = false;
		goto Done;
	}

	label = json_object_object_get(root, "displayName");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "DisplayName is missing");
		success = false;
		goto Done;
	}
	dbSearchItem.displayName = json_object_get_string(label);

	label = json_object_object_get(root, "displayFields");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "displayFields is missing");
		success = false;
		goto Done;
	}
	dbSearchItem.displayFields = json_object_get_string(label);

	label = json_object_object_get(root, "url");
	if(label && !is_error(label)) {
		dbSearchItem.url = json_object_get_string(label);
	}

	label = json_object_object_get(root, "launchParam");
	if(label && !is_error(label)) {
		dbSearchItem.launchParam = json_object_get_string(label);
	}

	label = json_object_object_get(root, "launchParamDbField");
	if(label && !is_error(label)) {
		dbSearchItem.launchParamDbField = json_object_get_string(label);
	}
	
	label = json_object_object_get(root, "batchQuery");
	if (!label || is_error(label)) {
		dbSearchItem.batchQuery = false;
	}
	else 
		dbSearchItem.batchQuery = json_object_get_boolean(label);

	label = json_object_object_get(root, "iconFilePath");
	if (label && !is_error(label)) {
		imageFilePath = json_object_get_string(label);
						
		if(USUtils::doesExistOnFilesystem(imageFilePath.c_str())) {
			dbSearchItem.iconFilePath = imageFilePath;
		}
	}
	
	//It passes the validation, add it to the list.
	if(overwrite && replaceItem) {
		it=m_mojodbSearchItemList.begin();
		std::advance(it, itemIndex);
		m_mojodbSearchItemList.insert(it, dbSearchItem);
	}
	else
		m_mojodbSearchItemList.push_back(dbSearchItem);

	//Save it to the Database.
	if(dbSync)
		dbHandler->addDBSearchRecord(dbSearchItem.id.c_str(), "dbsearch", dbSearchItem.displayName.c_str(), dbSearchItem.iconFilePath.c_str(), dbSearchItem.url.c_str(), 
				dbSearchItem.launchParam.c_str(), dbSearchItem.launchParamDbField.c_str(), dbSearchItem.dbQuery.c_str(), dbSearchItem.displayFields.c_str(), dbSearchItem.batchQuery?1:0, dbSearchItem.enabled?1:0, dbSearchItem.version);
	

	Done:

		if (root && !is_error(root))
			json_object_put(root);
		
		if(!success)
			return false;

		
	return true;
	
}

bool SearchItemsManager::modifyDBSearchItem(const char* jsonStr)
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label = NULL;
	std::string id;
	bool success = true;
	bool enabled;
	int index = 0;
						
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse content into json");
		success = false;
		goto Done;
	}
				
	label = json_object_object_get(root, "id");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "Id is missing");
		success = false;
		goto Done;
	}
	id = json_object_get_string(label);
			
	label = json_object_object_get(root, "enabled");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "enabled is missing");
		success = false;
		goto Done;
	}
	enabled = json_object_get_boolean(label);
		
	//Iterate the list to find the matching object.
	for(MojoDBSearchItemList::iterator it=m_mojodbSearchItemList.begin(); it!=m_mojodbSearchItemList.end(); ++it,index++) {
		MojoDBSearchItem& dbSearchItem =  (*it);
		if(dbSearchItem.id == id) {
			dbSearchItem.enabled = enabled;
			dbHandler->updateDBSearchRecord(id.c_str(), enabled?1:0);
			/*if(!enabled) {
				moveDBSearchItem(id, index, (int)m_mojodbSearchItemList.size());
			}*/
			break;
		}
	}
		
	Done:

		if (root && !is_error(root))
			json_object_put(root);

		if(!success)
			return false;

	return true;	
}

bool SearchItemsManager::modifyAllDBSearchItems(const char* jsonStr)
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label = NULL;
	bool success = true;
	bool enabled;

	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse content into json");
		success = false;
		goto Done;
	}

	label = json_object_object_get(root, "enabled");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "enabled is missing");
		success = false;
		goto Done;
	}
	enabled = json_object_get_boolean(label);

	//Iterate the list to find the matching object.
	for(MojoDBSearchItemList::iterator it=m_mojodbSearchItemList.begin(); it!=m_mojodbSearchItemList.end(); ++it) {
		MojoDBSearchItem& dbSearchItem =  (*it);
		dbSearchItem.enabled = enabled;
	}
	dbHandler->updateAllDBSearchRecord("dbsearch", enabled?1:0);

	Done:

		if (root && !is_error(root))
			json_object_put(root);

		if(!success)
			return false;

	return true;
}

bool SearchItemsManager::removeDBSearchItem(const char* jsonStr)
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label = NULL;
	std::string id;
	bool success = true;
						
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse content into json");
		success = false;
		goto Done;
	}
				
	label = json_object_object_get(root, "id");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "Id is missing");
		success = false;
		goto Done;
	}
	id = json_object_get_string(label);
				
	//Iterate the list to find the matching object.
	for(MojoDBSearchItemList::iterator it=m_mojodbSearchItemList.begin(); it!=m_mojodbSearchItemList.end(); ++it) {
		MojoDBSearchItem dbSearchItem =  (*it);
		if(dbSearchItem.id == id) {
			m_mojodbSearchItemList.erase(it);
			dbHandler->removeDBSearchRecord(id.c_str());
			break;
		}
	}

	Done:

		if (root && !is_error(root))
			json_object_put(root);

		if(!success)
			return false;
			
	return true;	
}

bool SearchItemsManager::reorderDBSearchItem(const char* jsonStr)
{
	json_object* root = json_tokener_parse(jsonStr);
	json_object* label = NULL;
	bool success = true;

	std::string id;
	int fromIndex, toIndex;
	
	if(!root || is_error(root)) {
		luna_critical(s_logChannel, "Failed to parse content into json");
		success = false;
		goto Done;
	}
		
	label = json_object_object_get(root, "id");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "ID is missing");
		success = false;
		goto Done;
	}
	id = json_object_get_string(label);
	
	label = json_object_object_get(root, "fromIndex");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "From Index is missing");
		success = false;
		goto Done;
	}
	fromIndex = json_object_get_int(label);
	
	if(fromIndex < 0 || fromIndex >= (int)m_mojodbSearchItemList.size()) {
		luna_critical(s_logChannel, "fromIndex is out of range");
		success = false;
		goto Done;
	}
	
	label = json_object_object_get(root, "toIndex");
	if (!label || is_error(label)) {
		luna_critical(s_logChannel, "To Index is missing");
		success = false;
		goto Done;
	}
	toIndex = json_object_get_int(label);
	
	//If an item moves downward, adjust the toIndex incrementing by 1.
	if(fromIndex < toIndex) {
		toIndex++;
	}
	
	success = moveDBSearchItem(id, fromIndex, toIndex);

	Done:

		if (root && !is_error(root))
			json_object_put(root);

		if(!success)
			return false;

	return true;
	
}

bool SearchItemsManager::moveDBSearchItem(const std::string& id, int fromIndex, int toIndex)
{
	
	MojoDBSearchItemList::iterator fromIt, toIt;
	
	//Make sure toIndex is within the range.
	if(toIndex > (int)m_mojodbSearchItemList.size())
		toIndex = (int)m_mojodbSearchItemList.size();
	
	fromIt = m_mojodbSearchItemList.begin();
	std::advance(fromIt, fromIndex);

	//Make sure it points the item that we are moving.
	if(fromIt->id != id) {
		return false;
	}

	toIt = m_mojodbSearchItemList.begin();
	std::advance(toIt, toIndex);
	
	m_mojodbSearchItemList.splice(toIt, m_mojodbSearchItemList, fromIt);

	//Sync the Database
	syncPrefDb();
	
	return true;
	
}

json_object* SearchItemsManager::getDBSearchItemList() 
{
	json_object* objArray = json_object_new_array();
	size_t found;
	
	for(MojoDBSearchItemList::const_iterator it=m_mojodbSearchItemList.begin(); it!=m_mojodbSearchItemList.end(); ++it) {
		
		MojoDBSearchItem dbSearchItem =  (*it);
		
		json_object* infoObj = json_object_new_object();
		
		json_object_object_add(infoObj,(char*) "id",json_object_new_string((char*) dbSearchItem.id.c_str()));
		json_object_object_add(infoObj,(char*) "displayName",json_object_new_string((char*) dbSearchItem.displayName.c_str()));
		json_object_object_add(infoObj,(char*) "iconFilePath",json_object_new_string((char*) dbSearchItem.iconFilePath.c_str()));
		json_object_object_add(infoObj,(char*) "launchParam",json_object_new_string((char*) dbSearchItem.launchParam.c_str()));
		json_object_object_add(infoObj,(char*) "launchParamDbField",json_object_new_string((char*) dbSearchItem.launchParamDbField.c_str()));
		json_object_object_add(infoObj,(char*) "dbQuery",json_tokener_parse((char*) dbSearchItem.dbQuery.c_str()));
		
		found = dbSearchItem.displayFields.find_first_of('[', 0);
		if(found != std::string::npos)
			json_object_object_add(infoObj,(char*) "displayFields", json_tokener_parse((char*) dbSearchItem.displayFields.c_str()));
		else
			json_object_object_add(infoObj,(char*) "displayFields", json_object_new_string((char*) dbSearchItem.displayFields.c_str()));
		json_object_object_add(infoObj,(char*) "batchQuery",json_object_new_boolean(dbSearchItem.batchQuery));
		json_object_object_add(infoObj,(char*) "enabled",json_object_new_boolean(dbSearchItem.enabled));
		
		json_object_array_add(objArray, infoObj);
		
	}
	return objArray;
}


bool SearchItemsManager::validateDbSearchItem(std::string appId, const char* dbQuery)
{
	json_object* fromObj = NULL;
	json_object* label = NULL;
	std::string dbKind;
	
	//No restriction for Palm Apps.
	if(appId.find("com.palm.",0) != std::string::npos) {
		return true;
	}
	
	fromObj = json_tokener_parse(dbQuery);
	
	label = json_object_object_get(fromObj, "from");
	if(!label || is_error(label))
		return false;
	dbKind = json_object_get_string(label);
	
	//Db Query should not contain com.palm in the db kind.
	if(dbKind.find("com.palm.",0) == std::string::npos)
		return true;
	
	return false;
}

bool SearchItemsManager::isItemExist(const std::string& id)
{
	for(SearchProvidersList::iterator it=m_searchProvidersList.begin(); it!=m_searchProvidersList.end(); ++it) {
		SearchProvider searchItem =  (*it);
		if(searchItem.id == id) {
			return true;
		}
	}
	
	for(ActionProvidersList::iterator it=m_actionProvidersList.begin(); it!=m_actionProvidersList.end(); ++it) {
		ActionProvider actionProvider =  (*it);
		if(actionProvider.id == id) {
			return true;
		}
	}
	
	for(MojoDBSearchItemList::iterator it=m_mojodbSearchItemList.begin(); it!=m_mojodbSearchItemList.end(); ++it) {
		MojoDBSearchItem dbSearch =  (*it);
		if(dbSearch.id == id) {
			return true;
		}
	}
	return false;
}

void SearchItemsManager::checkIntegrity()
{
	for(SearchProvidersList::iterator it=m_searchProvidersList.begin(); it!=m_searchProvidersList.end(); ++it) {
		SearchProvider searchItem =  (*it);
		if(searchItem.type == "app" && !searchItem.appExist && searchItem.id != "map") {
			dbHandler->removeSearchRecord(searchItem.id.c_str(), "search");
		}
	}

	for(ActionProvidersList::iterator it=m_actionProvidersList.begin(); it!=m_actionProvidersList.end(); ++it) {
		ActionProvider actionProvider =  (*it);
			if(!actionProvider.appExist) {
				dbHandler->removeSearchRecord(actionProvider.id.c_str(), "action");
			}
		}

	for(MojoDBSearchItemList::iterator it=m_mojodbSearchItemList.begin(); it!=m_mojodbSearchItemList.end(); ++it) {
		MojoDBSearchItem dbSearch =  (*it);
		if(!dbSearch.appExist) {
			dbHandler->removeDBSearchRecord(dbSearch.id.c_str());
		}
	}
	m_searchProvidersList.remove_if(PredSearch());
	m_actionProvidersList.remove_if(PredAction());
	m_mojodbSearchItemList.remove_if(PredDbSearch());
}

void SearchItemsManager::dumpList() 
{
	
	//int len = json_object_array_length(searchListObj);
	luna_critical(s_logChannel, "List Array length::  %s", json_object_get_string(searchListObj));
	
}

void SearchItemsManager::dumpActionList() {
	for(ActionProvidersList::const_iterator it=m_actionProvidersList.begin(); it!=m_actionProvidersList.end(); ++it) {
			ActionProvider actionProvider =  (*it);
			const char* appId = actionProvider.id.c_str();
			//const char* displayName = actionProvider.displayName.c_str();
			luna_critical(s_logChannel, "Action Provider :: %s  %d ", appId, actionProvider.appExist);
		}
	/*for(MojoDBSearchItemList::const_iterator it=m_mojodbSearchItemList.begin(); it!=m_mojodbSearchItemList.end(); ++it) {
		MojoDBSearchItem dbSearch =  (*it);
		const char* appId = dbSearch.id.c_str();
		const char* displayName = dbSearch.displayName.c_str();
		const char* queryStr = dbSearch.dbQuery.c_str();
		const char* displayFields = dbSearch.displayFields.c_str();
		luna_critical(s_logChannel, "Action Provider :: %s  %s %s %s", appId, displayName, queryStr, displayFields);
	}*/
	/*for(SearchProvidersList::iterator it=m_searchProvidersList.begin(); it!=m_searchProvidersList.end(); ++it) {
			SearchProvider& searchProvider =  (*it);
			const char* appId = searchProvider.id.c_str();
			luna_critical(s_logChannel, "Search Item :: %s  ", appId);
		}*/
}

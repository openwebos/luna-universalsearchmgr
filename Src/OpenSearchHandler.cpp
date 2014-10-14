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

#include <string>
#include <sstream>
#include <iostream>
#include <glib.h>
#include <dirent.h>
#include <sys/types.h>
#include <lunaservice.h>
#include <cjson/json.h>
#include <libxml/xmlreader.h>
#include <unistd.h>
#include "OpenSearchHandler.h"
#include "SearchItemsManager.h"
#include "UniversalSearchService.h"
#include "USUtils.h"

#define GENRIC_ICON "/usr/lib/luna/system/luna-applauncher/images/search-icon-generic.png"

static OpenSearchHandler* s_instance = NULL;

OpenSearchHandler* OpenSearchHandler::instance() {
    if (!s_instance)
	s_instance = new OpenSearchHandler();

    return s_instance;
}

OpenSearchHandler::OpenSearchHandler() {
#if defined (TARGET_DESKTOP)
	m_searchPluginPath = std::string(getenv("HOME"))+std::string("/downloads/searchplugins");
#else
	m_searchPluginPath = std::string("/var/palm/data/universalsearchmgr/searchplugins");
#endif
	g_mkdir_with_parents (m_searchPluginPath.c_str(), 0755);

#if defined (TARGET_DESKTOP)
	m_searchPluginIconPath = std::string(getenv("HOME"))+std::string("/downloads/assets/");
#else
	m_searchPluginIconPath = std::string("/var/palm/data/universalsearchmgr/assets/");
#endif
	g_mkdir_with_parents (m_searchPluginIconPath.c_str(), 0755);
	
}

void OpenSearchHandler::scanExistingPlugins()
{
    DIR	*dir;
    struct dirent* pluginFile;
    if (m_searchPluginPath.empty()) {
	g_warning ("empty plugin path, ignoring");
	return;
    }

    dir = opendir (m_searchPluginPath.c_str());
    if (!dir) {
	g_warning ("Unable to open the directory %s", m_searchPluginPath.c_str());
	return;
    }

    while ((pluginFile = readdir(dir)) != NULL) {
	std::string fileName = std::string (pluginFile->d_name);

	if (fileName == "." || fileName == "..")
	    continue;

	std::string pluginFilePath = m_searchPluginPath + std::string ("/") + fileName;

	g_debug ("Scanning file %s", pluginFilePath.c_str());
	parseXml (pluginFilePath, true);
    }

    closedir (dir);
}

bool OpenSearchHandler::downloadXml (LSHandle* lshandle, const std::string& xmlUrl) 
{
    LSError lserror;
    LSErrorInit(&lserror);
    bool success = false;

    std::string xmlFileName = encodeUrlToFile(xmlUrl);
    
	json_object*  downloadReq = json_object_new_object();
    json_object_object_add (downloadReq, "target", json_object_new_string (xmlUrl.c_str()));
    json_object_object_add (downloadReq, "targetDir", json_object_new_string (m_searchPluginPath.c_str()));
    json_object_object_add (downloadReq, "targetFilename", json_object_new_string (xmlFileName.c_str()));
    json_object_object_add (downloadReq, "canHandlePause", json_object_new_boolean (true));
    json_object_object_add (downloadReq, "subscribe", json_object_new_boolean (true));

    success = LSCall (lshandle, "palm://com.palm.downloadmanager/download", json_object_to_json_string (downloadReq),
	    OpenSearchHandler::cbDownloadManagerUpdate, NULL, NULL, &lserror);
    json_object_put (downloadReq);
    
    if (!success) {
	g_warning ("Failure while requesting download");
	LSErrorPrint (&lserror, stderr);
	LSErrorFree(&lserror);
    }

    return success;
}

bool OpenSearchHandler::parseXml (const std::string& xmlFile, bool scanningDir)
{
    xmlDocPtr doc;
    xmlNodePtr cur;
    OpenSearchInfo info;
    bool itemExist = false;

    doc = xmlParseFile (xmlFile.c_str());

    if (doc == NULL) {
	g_warning ("Unable to parse file %s\n", xmlFile.c_str());
	return false;
    }

    info.id = xmlFile;

    cur = xmlDocGetRootElement (doc);

    if (cur == NULL || cur->name == NULL) {
	g_warning ("Empty document\n");
	xmlFreeDoc (doc);
	return false;
    }

    if (xmlStrcmp (cur->name, (const xmlChar*) "SearchPlugin")
	    && xmlStrcmp (cur->name, (const xmlChar*) "OpenSearchDescription")) {
	g_warning ("Document not of type SearchPlugin\n");
	xmlFreeDoc (doc);
	return false;
    }

    for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
    if(cur->name == NULL)
    	continue;
	if (!xmlStrcmp (cur->name, (const xmlChar*) "ShortName")) {
	    xmlChar* value = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
	    g_debug ("ShortName is %s\n", value);
	    if(value)
	    	info.displayName = (const char*) value;
	    xmlFree (value);
	}
	else if (!xmlStrcmp (cur->name, (const xmlChar*) "Image")) {
	    xmlChar* value = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);
	    if(value)
	    	info.imageData = (const char*) value;
	    xmlFree (value);
	}
	else if (!xmlStrcmp (cur->name, (const xmlChar*) "Url")) {
	    bool isSearchUrl = false;
	    bool isSuggestionUrl = false;

	    xmlChar* rel = xmlGetProp (cur, (const xmlChar*) "rel");
	    if (rel) {
		if (xmlStrcmp (rel, (const xmlChar*) "results"))
		    isSearchUrl = true;
		else if (xmlStrcmp (rel, (const xmlChar*) "suggestions"))
		    isSuggestionUrl = true;
		else {
		    g_warning ("unsupported rel value, ignoring this url");
		    xmlFree (rel);
		    continue;
		}
	    }
	    xmlFree (rel);

	    xmlChar* value = xmlGetProp (cur, (const xmlChar*)"type");
	    if (value) {
		g_debug ("Url type = %s", value);
		    if (!xmlStrcmp (value, (const xmlChar*)"text/html")) {
			isSearchUrl = true;
		    }
		    else if (!xmlStrcmp (value, (const xmlChar*)"application/x-suggestions+json")) {
			isSuggestionUrl = true;
		    }
		    else if (!xmlStrcmp (value, (const xmlChar*)"application/json") && !isSuggestionUrl) {
			// allowing this type if rel is set as long as one of the urls are set
		    }
		    else {
			// not interested in other types of urls
			g_warning ("unsupported type %s, ignoring this url", (const char*)value);
			xmlFree (value);
			continue;
		    }
		xmlFree (value);
	    }
	    else {
		// not interested in no type
		g_warning ("no type specified, ignoring this url");
		continue;
	    }


	    xmlChar* templateUrl = xmlGetProp (cur, (const xmlChar*)"template");
	    if (templateUrl) {
		xmlNodePtr urlChildren = cur->xmlChildrenNode;
		bool firstParamAdded = false;
		while (urlChildren) {
			if(urlChildren->name == NULL)
				continue;
		    if (!xmlStrcmp (urlChildren->name, (const xmlChar*)"Param")) {
			xmlChar* name = xmlGetProp (urlChildren, (const xmlChar*)"name");
			xmlChar* value = xmlGetProp (urlChildren, (const xmlChar*)"value");
			if (name && value) {
			    xmlChar* buf = new xmlChar [1024];
			    if (firstParamAdded) {
				xmlStrPrintf (buf, 1024, (const xmlChar*) "&%s=%s", name, value);
			    }
			    else {
				if (templateUrl[xmlStrlen (templateUrl)-1] != '?')
				    xmlStrPrintf (buf, 1024, (const xmlChar*) "?%s=%s", name, value);
				else
				    xmlStrPrintf (buf, 1024, (const xmlChar*) "%s=%s", name, value);
				firstParamAdded = true;
			    }
			    xmlFree (name);
			    xmlFree (value);

			    templateUrl = xmlStrcat (templateUrl, buf);
			    xmlFree (buf);

			}
		    }
		    urlChildren = urlChildren->next;
		}
		g_debug ("Url template = %s\n", templateUrl);
		if (isSearchUrl)
		    info.searchUrl = (const char*) templateUrl;
		else if (isSuggestionUrl)
		    info.suggestionUrl = (const char*) templateUrl;
	    }
	    else {
		g_warning ("no template specified, ignoring this url");
		continue;
	    }
	}
    }
   
    g_debug ("search info -\n\tid: %s\n\tdisplayName: %s\n\tsearchUrl: %s\n\tsuggestionUrl: %s\n\timageData: %s\n",
	    info.id.c_str(), info.displayName.c_str(), info.searchUrl.c_str(), info.suggestionUrl.c_str(), info.imageData.c_str());

    if (info.id.empty() || info.displayName.empty() || info.searchUrl.empty()) {
	g_debug ("discarding info");
	return false;
    }
    
    //If it is already exist in the list then replace 
    if (SearchItemsManager::instance()->isSearchItemExist(info.id)) {
	// need to change this to update information instead
	g_debug ("update the search item info");
	SearchItemsManager::instance()->replaceSearchItem(info.id, info.searchUrl, info.suggestionUrl, info.displayName);
    }
    
    if(!info.imageData.empty()) {
    //Analyze the imageData to check if it's a url for an icon or an actual image data.
    char* uriScheme = g_uri_parse_scheme(info.imageData.c_str());
    if(uriScheme != NULL && strcmp(uriScheme, "data") == 0) {
    	//It's an image data...
    	//Check we have processed this image already
    	std::string	fileAndPath = m_searchPluginIconPath;
    	fileAndPath += info.id.substr (info.id.find_last_of ('/')+1, info.id.size() - 1);
    	fileAndPath += ".ico";
    	
    	 if(USUtils::doesExistOnFilesystem(fileAndPath.c_str())) {
    		//It exist. skip the download.
    		g_debug ("Icon File exist. skipping processImage");
    		info.imageData = fileAndPath;
   		 }
   		 else {
   		 	info.imageData = parseImage(info.id, info.imageData);
   		 }    	
    }
    else if(uriScheme != NULL && ((strcmp(uriScheme, "http") == 0) || (strcmp(uriScheme, "https") == 0))) {
    	//It's a url to icon. Download the image. But we don't wait for the download to complete.
    	//So, the assumption here is that download will complete and file will be created in the specified path. If there is a problem then it will default to generic icon.
    	info.imageData = downloadIcon(info.imageData);
    }
    else {
    	info.imageData = GENRIC_ICON;
    }
    }
    else {
    	info.imageData = GENRIC_ICON;
    }
    
    //Check to see if we have this item already in the optional list
	if (m_osItems.find (info.id) != m_osItems.end()) {
		itemExist = true;
	}
    g_debug ("adding info to m_osItems");
    m_osItems[info.id] = info;
    //Do not notify the SystemUI if we are adding these items during boot up. The Dashboard should only be displayed when we download the xml for the first time.
    if(!scanningDir && !itemExist)
    	notifyOpenSearchItemAvailable(info.displayName);

    return true;
}

const char* OpenSearchHandler::parseImage(const std::string& id, const std::string& imageData) 
{
	std::string imageOnly;
	std::string imageType;
	std::size_t found;
	std::string fileName;
	std::string Hex;
	gsize datalength;
	guchar* imgData;
	
	fileName = m_searchPluginIconPath;
    fileName += id.substr (id.find_last_of ('/')+1, id.size() - 1);
    fileName += ".ico";

	
	found = imageData.find_first_of(",");
	if(found == std::string::npos)
		return "";
	
	imageType = imageData.substr(0, found);
	imageOnly = imageData.substr(found+1);
	datalength = imageOnly.size();
	
	if(imageType.empty() || imageOnly.empty())
		return GENRIC_ICON;
	
	if(strstr(imageType.c_str(), "base64") == NULL) {
		imgData = (guchar*)unescapeString((gchar*)imageOnly.c_str(), datalength);		
	}
	else {
		if(strstr(imageType.c_str(), "x-icon") != NULL)
			imgData = g_base64_decode(imageOnly.c_str(), &datalength);
		else
			return GENRIC_ICON;
	}
	
	if(imgData == NULL) 
		return GENRIC_ICON;
	
	g_debug("File Name is %s \n", fileName.c_str());
	FILE * ofile = fopen(fileName.c_str(),"wb");
	if(ofile == NULL) {
		g_debug("File Open error");
		imgData = NULL;
		return GENRIC_ICON;
	}
	
	fwrite(imgData, 1, datalength, ofile );
	
	fflush(ofile);
	fclose(ofile);
	
	imgData = NULL;

	return fileName.c_str();
}

gchar* OpenSearchHandler::unescapeString (const gchar *escaped, gsize& size)
{
	gchar* res = (gchar*) malloc(size);
	int resSize = 0;
	       
	const gchar* src = escaped;
	gchar* dst = res;
	//g_debug("Escaped string %s %d\n", src, size);
	while (size > 0) {

	    if (*src != '%') {
	         *dst++ = *src++;
	         resSize++;
	         size--;
	    }
	    else {
	    	if (size < 3) {
	    		g_debug("data buffer too short");
	    		free(res);
	    		size = 0;
	    		return 0;
	    	}
            src++;
            size -= 3;
            int a = g_ascii_xdigit_value(*src++);
            int b = g_ascii_xdigit_value(*src++);
            if (a < 0 || b < 0) {
                 g_debug("invalid characters encountered in data buffer");
                 free(res);
                 size = 0;
                 return 0;
            }
            *dst++ = (a << 4) | b;
             resSize++;
	    }
	}
    size = resSize;
    return res;
}



json_object*	OpenSearchHandler::getOpenSearchList()
{
    json_object* searchList = json_object_new_object();

    json_object* osArray = json_object_new_array();
    for (std::map<std::string, OpenSearchInfo>::iterator it = m_osItems.begin(); it != m_osItems.end(); ++it) {
        if (SearchItemsManager::instance()->isSearchItemExist(it->second.id)) {
        	continue;
        }
	json_object* osElem = json_object_new_object();
	json_object_object_add (osElem, "id", json_object_new_string (it->second.id.c_str()));
	json_object_object_add (osElem, "displayName", json_object_new_string (it->second.displayName.c_str()));
	json_object_object_add (osElem, "searchUrl", json_object_new_string (it->second.searchUrl.c_str()));
	json_object_object_add (osElem, "suggestionUrl", json_object_new_string (it->second.suggestionUrl.c_str()));
	json_object_object_add (osElem, "imageData", json_object_new_string (it->second.imageData.c_str()));
	json_object_array_add (osArray, osElem);
    }
    json_object_object_add (searchList, "Options", osArray);
    return searchList;
}

bool 	OpenSearchHandler::clearOpenSearchList()
{
	bool removedSearchItem = false;
	
	for (std::map<std::string, OpenSearchInfo>::iterator it = m_osItems.begin(); it != m_osItems.end(); ) {
		std::map<std::string, OpenSearchInfo>::iterator erase_item = it++;
		if (SearchItemsManager::instance()->isSearchItemExist(erase_item->second.id)) {
			if(SearchItemsManager::instance()->removeDisabledOpenSearchItem(erase_item->second.id))
				removedSearchItem = true;
			else
				continue;
	    }
		
		//delete the icon files in the assets directory
		unlink(erase_item->second.imageData.c_str());
		
		//removing xml file
		g_debug ("Removing file %s", erase_item->second.id.c_str());
		unlink (erase_item->second.id.c_str());
		
		//remove it from the map.
		m_osItems.erase(erase_item);
		
	}
	
	return removedSearchItem;

	//delete the icon files in the assets directory
	/*for (std::map<std::string, OpenSearchInfo>::iterator it = m_osItems.begin(); it != m_osItems.end(); ++it) {
		unlink(it->second.imageData.c_str());
	}
    m_osItems.clear();

    DIR	*dir;
    struct dirent* pluginFile;
    if (m_searchPluginPath.empty()) {
	g_warning ("empty plugin path, ignoring");
	return;
    }

    dir = opendir (m_searchPluginPath.c_str());
    if (!dir) {
	g_warning ("Unable to open the directory %s", m_searchPluginPath.c_str());
	return;
    }

    while ((pluginFile = readdir(dir)) != NULL) {
	std::string fileName = std::string (pluginFile->d_name);

	if (fileName == "." || fileName == "..")
	    continue;

	std::string pluginFilePath = m_searchPluginPath + std::string ("/") + fileName;

	g_debug ("Removing file %s", pluginFilePath.c_str());
	unlink (pluginFilePath.c_str());
    }

    closedir (dir);*/
}

bool	OpenSearchHandler::clearOpenSearchItem (const std::string id)
{
    if (m_osItems.find (id) != m_osItems.end()) {
	m_osItems.erase (id);
	unlink (id.c_str());
	return true;
    }

    return false;
}

bool OpenSearchHandler::cbDownloadManagerUpdate(LSHandle* lshandle, LSMessage *message, void *user_data) 
{
    LSError lserror;
    LSErrorInit(&lserror);
    bool success = false;

    const char* payload = NULL;
    json_object *root = NULL, *label = NULL;
    std::string errMsg;
    std::string xmlFile;
    std::string newFilePath;
    bool downloadComplete = true;

    payload = LSMessageGetPayload (message);
    if (!payload) {
	g_warning ("No payload, ignoring call");
	goto done;
    }

    g_debug ("Download update %s", payload);

    root = json_tokener_parse (payload);
    if (!root || is_error (root)) {
	g_warning ("Unable to parse payload, ignoring call");
	goto done;
    }

    label = json_object_object_get (root, "completed");
    if (!label || is_error (label)) {
	g_debug ("No completed param, download not complete, ignoring");
	goto done;
    }
    
    downloadComplete = json_object_get_boolean (label);
    if (!downloadComplete) {
	g_debug ("Download not completed yet, ignoring");
	goto done;
    }

    label = json_object_object_get (root, "target");
    if (!label || is_error (label)) {
	g_warning ("No target param, invalid response");
	goto done;
    }

    xmlFile = json_object_get_string (label);
    if (xmlFile.empty()) {
	g_warning ("invalid file");
	goto done;
    }
#if !defined (TARGET_DESKTOP)
    newFilePath = OpenSearchHandler::instance()->m_searchPluginPath;
    newFilePath += '/';
    newFilePath += xmlFile.substr (xmlFile.find_last_of ('/')+1, xmlFile.size() - 1);
	
	if(USUtils::fileCopy(xmlFile.c_str(), newFilePath.c_str()) == -1)
	{
		g_warning("File Copy error" );
		unlink (newFilePath.c_str());
		goto done;
	 }
	g_debug ("Removing file %s", xmlFile.c_str());
	unlink (xmlFile.c_str());
    xmlFile = newFilePath;
#endif

    success = OpenSearchHandler::instance()->parseXml (xmlFile, false);
    if (!success) {
	g_warning ("failure to parse the XML file");
	errMsg = "Unable to parse file";
	unlink (xmlFile.c_str());
    }

    g_debug ("Done parsing the file");
    
done:
    if (root)
	json_object_put (root);

    return true;
}

const char* OpenSearchHandler::downloadIcon (const std::string& imageUrl) 
{
    LSError lserror;
    LSErrorInit(&lserror);
    bool success = false;
    
    std::string iconFileName = encodeUrlToFile(imageUrl);
    std::string fileAndPath = m_searchPluginIconPath + iconFileName;
    
    //check if the image has been downloaded already. 
    if(USUtils::doesExistOnFilesystem(fileAndPath.c_str())) {
    	//It exist. skip the download.
    	g_debug ("Icon File exist. adding info to m_osItems");
    	return fileAndPath.c_str();
    }
    
    json_object*  downloadReq = json_object_new_object();
    json_object_object_add (downloadReq, "target", json_object_new_string (imageUrl.c_str()));
    json_object_object_add (downloadReq, "targetDir", json_object_new_string (m_searchPluginIconPath.c_str()));
    json_object_object_add (downloadReq, "targetFilename", json_object_new_string (iconFileName.c_str()));
    json_object_object_add (downloadReq, "canHandlePause", json_object_new_boolean (true));
    json_object_object_add (downloadReq, "subscribe", json_object_new_boolean (true));

    success = LSCall (UniversalSearchService::instance()->getServiceHandle(), "palm://com.palm.downloadmanager/download", json_object_to_json_string (downloadReq),
	    OpenSearchHandler::cbDownloadManagerIconUpdate, NULL, NULL, &lserror);
    json_object_put (downloadReq);
    g_debug("Download request sent out for downloading an icon");
    if (!success) {
	g_warning ("Failure while requesting icon download");
	LSErrorPrint (&lserror, stderr);
	LSErrorFree(&lserror);
	return GENRIC_ICON;
    }
    
    return fileAndPath.c_str();
}

bool OpenSearchHandler::cbDownloadManagerIconUpdate(LSHandle* lshandle, LSMessage *message, void *user_data) 
{
    LSError lserror;
    LSErrorInit(&lserror);
    bool success = false;
  
    const char* payload = NULL;
    json_object *root = NULL, *label = NULL;
    std::string errMsg;
    std::string xmlFile;
    std::string newFilePath;
    bool downloadComplete = false;

    payload = LSMessageGetPayload (message);
    if (!payload) {
	g_warning ("No payload, ignoring call");
	goto done;
    }

    g_debug ("Download update %s", payload);

    root = json_tokener_parse (payload);
    if (!root || is_error (root)) {
	g_warning ("Unable to parse payload, ignoring call");
	goto done;
    }

    label = json_object_object_get (root, "completed");
    if (!label || is_error (label)) {
	g_debug ("No completed param, download not complete, ignoring");
	goto done;
    }
    
    downloadComplete = json_object_get_boolean (label);
    if (!downloadComplete) {
	g_debug ("Download not completed yet, ignoring");
	goto done;
    }

    label = json_object_object_get (root, "target");
    if (!label || is_error (label)) {
	g_warning ("No target param, invalid response");
	goto done;
    }

    xmlFile = json_object_get_string (label);
    if (xmlFile.empty()) {
	g_warning ("invalid file");
	goto done;
    }
    else
    	success = true;
    
#if !defined (TARGET_DESKTOP)
    newFilePath = OpenSearchHandler::instance()->m_searchPluginIconPath;
    newFilePath += xmlFile.substr (xmlFile.find_last_of ('/')+1, xmlFile.size() - 1);

	if(USUtils::fileCopy(xmlFile.c_str(), newFilePath.c_str()) == -1)
	{
		g_warning("File Copy error" );
		success = false;
		goto done;
	 }
	g_debug ("Removing file %s", xmlFile.c_str());
	unlink (xmlFile.c_str());
    xmlFile = newFilePath;
#endif
    
done:
    if (root)
    	json_object_put (root);
    
    return true;
}

bool OpenSearchHandler::notifyOpenSearchItemAvailable(std::string& displayName)
{
	LSError lserror;
    LSErrorInit(&lserror);
    bool success = false;
    
    json_object*  systemUIReq = json_object_new_object();
    json_object* message = json_object_new_object();
    json_object_object_add (message, "displayName", json_object_new_string (displayName.c_str()));
    
    json_object_object_add (systemUIReq, "event", json_object_new_string ("openSearchEngineAvailable"));
    json_object_object_add (systemUIReq, "message", json_object_get(message));

    success = LSCall (UniversalSearchService::instance()->getServiceHandle(), "palm://com.palm.systemmanager/publishToSystemUI", json_object_to_json_string (systemUIReq),
	    NULL, NULL, NULL, &lserror);
    json_object_put (systemUIReq);
    json_object_put (message);
    if (!success) {
	g_warning ("Failure while requesting download");
	LSErrorPrint (&lserror, stderr);
	LSErrorFree(&lserror);
    }
    UniversalSearchService::instance()->postOptionalSearchListChange();
    return success;

}

bool OpenSearchHandler::checkForDuplication(std::string& xmlFileName)
{
	std::string id = m_searchPluginPath + "/" + encodeUrlToFile(xmlFileName);

	if (m_osItems.find (id) != m_osItems.end()) {
		return true;
	}
	return false;
}

int OpenSearchHandler::getOptionalListSize() {
	
	return (int) m_osItems.size();
}

std::string OpenSearchHandler::encodeUrlToFile (const std::string& source)
{
    std::string fileName = source;

    // Simple solution to replace non alphanumeric characters with a safe '.'
    // failed attempts to do this differently below
    unsigned i = 0;
    for (i  = 0; i != fileName.length(); ++i) {
	if (!g_ascii_isalnum (fileName[i]))
	    fileName[i] = '.';
    }

    return fileName;

#if 0
    // XXX: the resulting file name did not work with the xmlReader

    // using uriparser
    char * buffer = new char[6*(source.length())+1];
    char * term = uriEscapeA(source.c_str(),buffer,false,false);
    if (term == NULL)
	return std::string("");

    // escaping the % characters
    gchar* escapedBuf = g_strescape (buffer, NULL);
    std::string r(escapedBuf);
    g_debug ("%s: Original=%s\n New=%s", __PRETTY_FUNCTION__, source.c_str(), r.c_str());
    delete [] buffer;
    g_free (escapedBuf);
    return r;
#endif

}



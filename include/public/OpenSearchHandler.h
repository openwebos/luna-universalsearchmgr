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

#ifndef OPENSEARCHHANDLER_H
#define OPENSEARCHHANDLER_H

#include <string>
#include <map>

class OpenSearchHandler {
    public:
	static OpenSearchHandler* instance();

	struct OpenSearchInfo {
	    std::string id;
	    std::string displayName;
	    std::string searchUrl;
	    std::string suggestionUrl;
	    std::string imageData;
	};

	bool	parseXml (const std::string& xmlFile, bool scanningDir);
	bool	downloadXml (LSHandle* lshandle, const std::string& xmlUrl);
	const char* 	downloadIcon (const std::string& imageUrl);
	const char* 	parseImage(const std::string& id, const std::string& imageData);
	gchar* 	unescapeString (const gchar *escaped, gsize& size);
	bool 	checkForDuplication(std::string& xmlFileName);
	int 	getOptionalListSize();
	bool 	notifyOpenSearchItemAvailable(std::string& displayName);

	json_object*	getOpenSearchList();
	bool		clearOpenSearchList();
	bool		clearOpenSearchItem (const std::string id);

	static bool cbDownloadManagerUpdate(LSHandle* lshandle, LSMessage *message, void *user_data);
	static bool cbDownloadManagerIconUpdate(LSHandle* lshandle, LSMessage *message, void *user_data);
	void		scanExistingPlugins();
	
    private:
	OpenSearchHandler();

	std::string	encodeUrlToFile (const std::string& url);

	std::string m_searchPluginPath;
	std::map<std::string, OpenSearchInfo> m_osItems;
	std::string m_searchPluginIconPath;
};

#endif // OPENSEARCHHANDLER_H

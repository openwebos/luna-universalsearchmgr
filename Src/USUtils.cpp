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

#include "USUtils.h"
#include <sys/stat.h>
#include <sys/statfs.h>
#include <glib.h>

namespace USUtils {

char* readFile(const char* filePath)
{
	if (!filePath)
		return 0;
	
	FILE* f = fopen(filePath,"r");
	
	if (!f)
		return 0;

	fseek(f, 0L, SEEK_END);
	long sz = ftell(f);
	fseek( f, 0L, SEEK_SET );
	if (!sz || sz == -1L) {
		fclose(f);
		return 0;
	}

	char* ptr = new char[sz+1];
	if( !ptr )
	{
		fclose(f);
		return 0;
	}
	ptr[sz] = 0;
	
	size_t res = fread(ptr, sz, 1, f);
	fclose(f);

	return ptr;
}

bool doesExistOnFilesystem(const char * pathAndFile) {
	
	if (pathAndFile == NULL)
		return false;
	
	std::string fName(pathAndFile);
	std::string newPathAndFile;

	struct stat buf;
	if (-1 == ::stat(pathAndFile, &buf ) ) {
		//Check if it is in 1.5 folder.
		std::string::size_type pos = fName.find_last_of("/");
		if(pos != std::string::npos) {
			newPathAndFile = fName.substr(0, pos) + "/1.5/" + fName.substr(pos);
			if(-1 != ::stat(newPathAndFile.c_str(), &buf) )
				return true;
		}
		return false;
	}
	return true;
			
	
}

int getUniqueId() {
	return rand() % 30000 + 1;
}

void initRandomGenerator() {
	srand(time(NULL));
}

bool getServiceDomainPart(const std::string& url, std::string& domainPart) 
{
	const std::string delimiter="/";
	if(url.empty())
		return false;
	
	//First look for "://" in the service url.
	std::string::size_type pos = url.find_first_of(':', 0);
	if(pos != std::string::npos) {
		//check the 2 chars followed by are "//"
		if(url[pos+1] == '/' && url[pos+2] == '/')
			pos += 3;
	}
	else
		pos = 0;
	
	//Find the first occurrence of "/"
	std::string::size_type lastPos = url.find_first_of(delimiter, pos);
	if(lastPos != std::string::npos) {
		domainPart = url.substr(pos, lastPos);
		return true;
	}
	
	return false;
}

int fileCopy(const char * srcFileAndPath,const char * dstFileAndPath)
{
	if ((srcFileAndPath == NULL) || (dstFileAndPath == NULL))
		return -1;
	
	FILE * infp = fopen(srcFileAndPath,"rb");
	FILE * outfp = fopen(dstFileAndPath,"wb");
	if ((infp == NULL) || (outfp == NULL)) {
		if (infp)
			fclose(infp);
		if (outfp)
			fclose(outfp);
		return -1;
	}
	
	char buffer[2048];
	while (!feof(infp)) {
		size_t r = fread(buffer,1,2048,infp);
		if ((r == 0) && (ferror(infp))) {
			break;
		}
		size_t w = fwrite(buffer,1,r,outfp);
		if (w < r) {
			break;
		}
	}
	
	fflush(infp);
	fflush(outfp);
	fclose(infp);
	fclose(outfp);
	return 1;
}
} // End of namespace USUtils

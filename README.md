Summary
=======
Open webOS "Just Type" service component, supporting the Universal Search feature.

LunaUniversalSearchMgr
======================

This component supports the following methods, which are described in detail in the generated documentation:  

*  com.palm.universalsearch/addOptionalSearchDesc
*  com.palm.universalsearch/addSearchItem
*  com.palm.universalsearch/clearOptionalSearchList
*  com.palm.universalsearch/getAllSearchPreference
*  com.palm.universalsearch/getOptionalSearchList
*  com.palm.universalsearch/getSearchPreference
*  com.palm.universalsearch/getUniversalSearchList
*  com.palm.universalsearch/getVersion
*  com.palm.universalsearch/removeOptionalSearchItem
*  com.palm.universalsearch/removeSearchItem
*  com.palm.universalsearch/reorderSearchItem
*  com.palm.universalsearch/setSearchPreference
*  com.palm.universalsearch/updateAllSearchItems
*  com.palm.universalsearch/updateSearchItem

How to Build on Linux
=====================

### Building the latest "stable" version

Clone the repository at http://www.github.com/openwebos/build-webos and follow the instructions in that README to build Open webOS.

To build or rebuild just this component, if your build-webos directory is ~/openwebos/build-webos, use:

    cd ~/openwebos/build-webos
    make cleanall-luna-universalsearchmgr luna-universalsearchmgr
    
The resulting IPK package will be in your BUILD directory, under deploy/ipk/<architecture>, such as this example:

    ~/openwebos/build-webos/BUILD-qemux86/deploy/ipk/i586/luna-universalsearchmgr_2.0.0-1.00-r5_i586.ipk
    
You can transfer this to your existing image, and install it by logging into the Open webOS system, and using:

    ipkg install /path/to/luna-universalsearchmgr_2.0.0-1.00-r5_i586.ipk
    
Or you can create a completely new Open webOS image with:

    make clean-webos-image webos-image
    
### Building your local clone

After successfully building the latest stable version, you may configure build-webos to build this component from your own local clone.

You can specify what directory to use as the local source inside the file "global-webos.conf" in your home directory, or within the file "webos-local.conf" within the build-webos directory, by adding the following:

    S_pn-luna-universalsearchmgr = "/path/to/my/luna-universalsearchmgr"
    
Then follow the instructions above to rebuild and install this package.

# Copyright and License Information

All content, including all source code files and documentation files in this repository except otherwise noted are: 

 Copyright (c) 2010-2012 Hewlett-Packard Development Company, L.P.

All content, including all source code files and documentation files in this repository except otherwise noted are:
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this content except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.


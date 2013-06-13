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

#include<cstdlib>
#include<glib.h>

#include "UniversalSearchService.h"

GMainLoop* gMainLoop = NULL;

int main( int argc, char** argv)
{
    gMainLoop = g_main_loop_new (NULL, FALSE);
    g_thread_init(NULL);
   
    // Initialize the Universal Search Manager
    UniversalSearchService::instance();

    g_main_loop_run (gMainLoop);

    return 0;
}

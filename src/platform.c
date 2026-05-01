/*
 *   Copyright 2026 Franciszek Balcerak
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifdef _WIN32
	#include "windows.c"
#elif defined(__linux__)
	#include "linux.c"
#else
	#error Unsupported platform.
#endif

#include <string.h>


const char*
alloc_get_env(
	const char* name
	)
{
	assert_not_null(name);

	const char** env = alloc_get_env_array();
	assert_not_null(env);

	while(*env)
	{
		size_t len = strlen(name);
		if(strncmp(*env, name, len) == 0)
		{
			const char* value = *env + len;

			if(*value == '=')
			{
				++value;
			}

			return value;
		}

		++env;
	}

	return NULL;
}


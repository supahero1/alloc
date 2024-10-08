/*
 *   Copyright 2024 Franciszek Balcerak
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

#ifdef __cplusplus
extern "C" {
#endif

#include "../include/debug.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef __linux__
	#include <execinfo.h>
#endif


Static void
PrintStackTrace(
	void
	)
{
#ifdef __linux__
	void* Buffer[256];
	int Count = backtrace(Buffer, 256);
	char** Symbols = backtrace_symbols(Buffer, Count);

	fprintf(stderr, "Stack trace (%d):\n", Count);

	for(int i = 0; i < Count; ++i)
	{
		fprintf(stderr, "#%d:\t%s\n", i + 1, Symbols[i]);
	}

	free(Symbols);
#else
	fprintf(stderr, "Stack trace not supported on this platform\n");
#endif
}


void
AssertFailed(
	const char* Msg1,
	const char* TypeA,
	const char* Msg2,
	const char* TypeB,
	const char* Msg3,
	...
	)
{
	char Fmt[4096];
	sprintf(Fmt, "%s%s%s%s%s", Msg1, TypeA, Msg2, TypeB, Msg3);

	va_list List;
	va_start(List, Msg3);
		vfprintf(stderr, Fmt, List);
	va_end(List);

	PrintStackTrace();

	abort();
}


void
UnreachableAssertFailed(
	const char* Msg
	)
{
	fprintf(stderr, "%s", Msg);

	PrintStackTrace();

	abort();
}


void
LocationLogger(
	const char* Msg,
	...
	)
{
	va_list List;
	va_start(List, Msg);
		vfprintf(stderr, Msg, List);
	va_end(List);

	PrintStackTrace();
}


#ifdef __cplusplus
}
#endif

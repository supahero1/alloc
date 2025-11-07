/*
 *   Copyright 2024-2025 Franciszek Balcerak
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

#include "../include/debug.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#if __has_include(<execinfo.h>)
	#define DEBUG_STACK_TRACE

	#include <execinfo.h>
#endif


private void
print_stack_trace(
	void
	)
{
#ifdef DEBUG_STACK_TRACE
	void* buffer[256];
	int count = backtrace(buffer, 256);
	char** symbols = backtrace_symbols(buffer, count);

	fprintf(stderr, "Stack trace (%d):\n", count);

	for(int i = 0; i < count; ++i)
	{
		fprintf(stderr, "#%d:\t%s\n", i + 1, symbols[i]);
	}

	free(symbols);
#else
	fprintf(stderr, "Stack trace not supported on this platform\n");
#endif
}


void
assert_failed(
	const char* msg1,
	const char* type1,
	const char* msg2,
	const char* type2,
	const char* msg3,
	...
	)
{
	char format[4096];
	sprintf(format, "%s%s%s%s%s", msg1, type1, msg2, type2, msg3);

	va_list list;
	va_start(list, msg3);
		vfprintf(stderr, format, list);
	va_end(list);

	print_stack_trace();

	abort();
}


void
unreachable_assert_failed(
	const char* msg
	)
{
	fprintf(stderr, "%s", msg);

	print_stack_trace();

	abort();
}


void
location_logger(
	const char* msg,
	...
	)
{
	va_list list;
	va_start(list, msg);
		vfprintf(stderr, msg, list);
	va_end(list);

	print_stack_trace();
}

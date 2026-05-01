/*
 *   Copyright 2024-2026 Franciszek Balcerak
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

#include <alloc/debug.h>
#include <alloc/macro.h>

#include <dlfcn.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <execinfo.h>

#if __has_include(<execinfo.h>)
	#define DEBUG_STACK_TRACE

#endif


void
print_stack_trace(
	void
	)
{
#ifdef DEBUG_STACK_TRACE
	void* buffer[256];
	int count = backtrace(buffer, 256);

	write(STDERR_FILENO, "Raw stack trace:\n", 17);
	backtrace_symbols_fd(buffer, count, STDERR_FILENO);

	char** symbols = backtrace_symbols(buffer, count);

	char out[16 * 1024];
	size_t off = 0;
	size_t cap = sizeof(out);
	int width = snprintf(NULL, 0, "%d", count);

	off += snprintf(out + off, cap - off, "Stack trace (%d):\n", count);

	char line[512];
	char cmd[512];

	for(int i = 0; i < count; ++i)
	{
		int num = i + 1;
		int digits = snprintf(NULL, 0, "%d", num);

		off += snprintf(out + off, cap - off, "#%d:", num);

		for(int s = 0; s < width - digits + 1 && off < cap; ++s)
		{
			out[off++] = ' ';
		}

		int ok = 0;

		Dl_info info;
		if(dladdr(buffer[i], &info) && info.dli_fname)
		{
			void* offset = (void*)(buffer[i] - info.dli_fbase);
			snprintf(cmd, sizeof(cmd), "addr2line -f -p -e %s %p 2>/dev/null", info.dli_fname, offset);

			FILE* fp = popen(cmd, "r");
			if(fp)
			{
				if(fgets(line, sizeof(line), fp))
				{
					ok = line[0] != '?';
				}

				pclose(fp);
			}
		}

		off += snprintf(out + off, cap - off, ok ? "%s" : "%s\n", ok ? line : symbols[i]);
		if(off >= cap)
		{
			break;
		}
	}

	out[cap - 1] = '\0';

	fprintf(stderr, "%s", out);

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

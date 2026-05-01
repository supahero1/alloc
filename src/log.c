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

#include <alloc/log.h>
#include <alloc/macro.h>
#include <alloc/types.h>
#include <alloc/atomic.h>
#include <alloc/consts.h>
#include <alloc/platform.h>

#include <stdio.h>
#include <stdarg.h>


typedef enum alloc_log_color
{
	ALLOC_LOG_COLOR_DEFAULT		= 0,
	ALLOC_LOG_COLOR_RED			= 31,
	ALLOC_LOG_COLOR_GREEN		= 32,
	ALLOC_LOG_COLOR_YELLOW		= 33,
	ALLOC_LOG_COLOR_BLUE		= 34,
	ALLOC_LOG_COLOR_MAGENTA		= 35,
	ALLOC_LOG_COLOR_CYAN		= 36,
}
alloc_log_color_t;


typedef struct alloc_log
{
	const char* _Atomic tag;
}
alloc_log_t;


alloc_log_t alloc_log =
{
	.tag = ALLOC_LOG_TAG_DEFAULT
};


alloc_log_color_t
alloc_log_verbosity_color(
	alloc_log_verbosity_t verbosity
	)
{
	assert_lt(verbosity, ALLOC_LOG_VERBOSITY__COUNT);


	switch(verbosity)
	{

	case ALLOC_LOG_VERBOSITY_ERROR:		return ALLOC_LOG_COLOR_RED;
	case ALLOC_LOG_VERBOSITY_WARNING:	return ALLOC_LOG_COLOR_YELLOW;
	case ALLOC_LOG_VERBOSITY_INFO:		return ALLOC_LOG_COLOR_GREEN;
	case ALLOC_LOG_VERBOSITY_DEBUG:		return ALLOC_LOG_COLOR_CYAN;
	default: hard_assert_unreachable();

	}
}


void
alloc_log_build_fmt(
	char* buf,
	int max_len,
	int count,
	...
	)
{
	va_list args;
	va_start(args, count);

	char* ptr = buf;
	char* end = buf + max_len - 1;

	while(count-- > 0 && ptr < end)
	{
		char* fmt = va_arg(args, char*);
		while(*fmt && ptr < end)
		{
			*ptr++ = *fmt++;
		}
	}

	*ptr = 0;

	va_end(args);
}


void
alloc_log_set_tag(
	const char* tag
	)
{
	atomic_store_rx(&alloc_log.tag, tag ? tag : ALLOC_LOG_TAG_DEFAULT);
}


void
alloc_do_log(
	alloc_t fd,
	alloc_log_verbosity_t verbosity,
	const char* format,
	...
	)
{
	char final_format[4096];
	const char* tag = atomic_load_rx(&alloc_log.tag);

	if(alloc_is_tty(fd))
	{
		snprintf(final_format, sizeof(final_format),
			"\033[%dm%s\033[0m %s\n", alloc_log_verbosity_color(verbosity), tag, format);
	}
	else
	{
		snprintf(final_format, sizeof(final_format), "%s %s\n", tag, format);
	}

	va_list args;
	va_start(args, format);
		vdprintf(fd, final_format, args);
	va_end(args);
}

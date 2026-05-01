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

#pragma once

#include <alloc/macro.h>
#include <alloc/types.h>


typedef enum alloc_consts_verbosity
{
	ALLOC_LOG_VERBOSITY_ERROR	= 0,
	ALLOC_LOG_VERBOSITY_WARNING	= 1,
	ALLOC_LOG_VERBOSITY_INFO	= 2,
	ALLOC_LOG_VERBOSITY_DEBUG	= 3,
	MACRO_ENUM_BITS(ALLOC_LOG_VERBOSITY)
}
alloc_log_verbosity_t;


extern void
alloc_log_build_fmt(
	char* buf,
	int max_len,
	int count,
	...
	);


extern void
alloc_log_set_tag(
	const char* tag
	);


extern void
alloc_do_log(
	alloc_t fd,
	alloc_log_verbosity_t verbosity,
	const char* format,
	...
	);


#define alloc_log(fd, bypass, _verbosity, ...)							\
do																		\
{																		\
	if(!bypass && attr_likely(_verbosity > alloc_consts.log.verbosity))	\
	{																	\
		break;															\
	}																	\
																		\
	char _fmt_buf[4096];												\
	alloc_log_build_fmt(_fmt_buf, sizeof(_fmt_buf),						\
		MACRO_COUNT(__VA_ARGS__), MACRO_FORMAT(__VA_ARGS__));			\
																		\
	alloc_do_log(fd, _verbosity, _fmt_buf, __VA_ARGS__);				\
}																		\
while(0)

#define ALLOC_DEFAULT_LOG_FD alloc_consts.log.fd

#define alloc_do_custom_log_debug(fd, ...) alloc_log(fd, 1, ALLOC_LOG_VERBOSITY_DEBUG, __VA_ARGS__)
#define alloc_do_custom_log_info(fd, ...) alloc_log(fd, 1, ALLOC_LOG_VERBOSITY_INFO, __VA_ARGS__)
#define alloc_do_custom_log_warning(fd, ...) alloc_log(fd, 1, ALLOC_LOG_VERBOSITY_WARNING, __VA_ARGS__)
#define alloc_do_custom_log_error(fd, ...) alloc_log(fd, 1, ALLOC_LOG_VERBOSITY_ERROR, __VA_ARGS__)

#define alloc_do_custom_log_tagged(fd, tag, _verbosity, ...)	\
do																\
{																\
	alloc_log_set_tag(tag);										\
	alloc_log(fd, 1, _verbosity, __VA_ARGS__);					\
	alloc_log_set_tag(NULL);									\
}																\
while(0)

#define alloc_do_custom_log_tagged_debug(fd, tag, ...)	\
alloc_do_custom_log_tagged(fd, tag, ALLOC_LOG_VERBOSITY_DEBUG, __VA_ARGS__)

#define alloc_do_custom_log_tagged_info(fd, tag, ...)	\
alloc_do_custom_log_tagged(fd, tag, ALLOC_LOG_VERBOSITY_INFO, __VA_ARGS__)

#define alloc_do_custom_log_tagged_warning(fd, tag, ...)	\
alloc_do_custom_log_tagged(fd, tag, ALLOC_LOG_VERBOSITY_WARNING, __VA_ARGS__)

#define alloc_do_custom_log_tagged_error(fd, tag, ...)	\
alloc_do_custom_log_tagged(fd, tag, ALLOC_LOG_VERBOSITY_ERROR, __VA_ARGS__)

#define alloc_do_log_debug(...) alloc_do_custom_log_debug(ALLOC_DEFAULT_LOG_FD, __VA_ARGS__)
#define alloc_do_log_info(...) alloc_do_custom_log_info(ALLOC_DEFAULT_LOG_FD, __VA_ARGS__)
#define alloc_do_log_warning(...) alloc_do_custom_log_warning(ALLOC_DEFAULT_LOG_FD, __VA_ARGS__)
#define alloc_do_log_error(...) alloc_do_custom_log_error(ALLOC_DEFAULT_LOG_FD, __VA_ARGS__)

#ifdef ALLOC_RELEASE
	#define alloc_log_debug(...)
	#define alloc_log_info(...)
#else
	#define alloc_log_debug(...) alloc_log(ALLOC_DEFAULT_LOG_FD, 0, ALLOC_LOG_VERBOSITY_DEBUG, __VA_ARGS__)
	#define alloc_log_info(...) alloc_log(ALLOC_DEFAULT_LOG_FD, 0, ALLOC_LOG_VERBOSITY_INFO, __VA_ARGS__)
#endif

#define alloc_log_warning(...) alloc_log(ALLOC_DEFAULT_LOG_FD, 0, ALLOC_LOG_VERBOSITY_WARNING, __VA_ARGS__)
#define alloc_log_error(...) alloc_log(ALLOC_DEFAULT_LOG_FD, 0, ALLOC_LOG_VERBOSITY_ERROR, __VA_ARGS__)

#include <alloc/consts.h>

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

#include <alloc/consts.h>

#if !defined(ALLOC_RELEASE) && __has_include(<valgrind/memcheck.h>)
	#define ALLOC_VALGRIND

	#include <valgrind/memcheck.h>
	#include <valgrind/valgrind.h>

	#define ALLOC_VALGRIND_DISABLE_ERROR_REPORTING()	\
	VALGRIND_DISABLE_ERROR_REPORTING
	#define ALLOC_VALGRIND_ENABLE_ERROR_REPORTING()	\
	VALGRIND_ENABLE_ERROR_REPORTING
	#define ALLOC_VALGRIND_ALLOC(ptr, _size, zero)	\
	VALGRIND_MALLOCLIKE_BLOCK(ptr, _size, alloc_consts.red_zone.size, zero)
	#define ALLOC_VALGRIND_RESIZE(ptr, old_size, new_size)	\
	VALGRIND_RESIZEINPLACE_BLOCK(ptr, old_size, new_size, alloc_consts.red_zone.size)
	#define ALLOC_VALGRIND_DEFINE(ptr, size)	\
	VALGRIND_MAKE_MEM_DEFINED(ptr, size)
	#define ALLOC_VALGRIND_FREE(ptr)	\
	VALGRIND_FREELIKE_BLOCK(ptr, alloc_consts.red_zone.size)
	#define ALLOC_VALGRIND_INTERNAL_ALLOC(ptr, size)	\
	VALGRIND_MALLOCLIKE_BLOCK(ptr, size, 0, 0)
	#define ALLOC_VALGRIND_INTERNAL_FREE(ptr)	\
	VALGRIND_FREELIKE_BLOCK(ptr, 0)
#else
	#define ALLOC_VALGRIND_DISABLE_ERROR_REPORTING()
	#define ALLOC_VALGRIND_ENABLE_ERROR_REPORTING()
	#define ALLOC_VALGRIND_ALLOC(ptr, size, zero)
	#define ALLOC_VALGRIND_RESIZE(ptr, old_size, new_size)
	#define ALLOC_VALGRIND_DEFINE(ptr, size)
	#define ALLOC_VALGRIND_FREE(ptr)
	#define ALLOC_VALGRIND_INTERNAL_ALLOC(ptr, size)
	#define ALLOC_VALGRIND_INTERNAL_FREE(ptr)
#endif

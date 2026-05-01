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

#include <alloc/attr.h>
#include <alloc/debug.h>
#include <alloc/macro.h>
#include <alloc/types.h>


extern attr_api void
alloc_reset(
	void
	);


extern attr_api void
alloc_thread_init(
	void
	);


extern attr_api void
alloc_update_numa(
	void
	);


extern attr_api attr_alloc_fn void*
alloc_alloc_e(
	alloc_t size,
	int zero
	);


#define alloc_alloc(ptr, size, zero)	\
alloc_alloc_e(sizeof(*(ptr)) * (size), zero)


attr_inline attr_alloc_fn void*
alloc_alloc_aligned_e(
	alloc_t size,
	alloc_t alignment,
	int zero
	)
{
	return alloc_alloc_e(MACRO_MAX(size, alignment), zero);
}


#define alloc_alloc_aligned(ptr, size, alignment, zero)	\
alloc_alloc_aligned_e(sizeof(*(ptr)) * (size), alignment, zero)


extern attr_api void
alloc_free_e(
	const volatile void* ptr,
	alloc_t size
	);


#define alloc_free(ptr, size)	\
alloc_free_e(ptr, sizeof(*(ptr)) * (size))


attr_inline void
alloc_free_aligned_e(
	const volatile void* ptr,
	alloc_t size,
	alloc_t alignment
	)
{
	alloc_free_e(ptr, MACRO_MAX(size, alignment));
}


#define alloc_free_aligned(ptr, size, alignment)	\
alloc_free_aligned_e(ptr, sizeof(*(ptr)) * (size), alignment)


extern attr_api void*
alloc_realloc_e(
	const volatile void* ptr,
	alloc_t old_size,
	alloc_t new_size,
	int zero
	);


#define alloc_realloc(ptr, old_size, new_size, zero)	\
alloc_realloc_e(ptr, sizeof(*(ptr)) * (old_size), sizeof(*(ptr)) * (new_size), zero)


attr_inline void*
alloc_realloc_aligned_e(
	const volatile void* ptr,
	alloc_t old_size,
	alloc_t old_alignment,
	alloc_t new_size,
	alloc_t new_alignment,
	int zero
	)
{
	return alloc_realloc_e(ptr, MACRO_MAX(old_size, old_alignment), MACRO_MAX(new_size, new_alignment), zero);
}


#define alloc_realloc_aligned(ptr, old_size, old_alignment, new_size, new_alignment, zero)	\
alloc_realloc_aligned_e(ptr, sizeof(*(ptr)) * (old_size), old_alignment, sizeof(*(ptr)) * (new_size), new_alignment, zero)


attr_inline attr_alloc_fn void*
alloc_malloc_aligned_e(
	alloc_t size,
	alloc_t alignment
	)
{
	return alloc_alloc_aligned_e(size, alignment, 0);
}


#define alloc_malloc_aligned(ptr, size, alignment)	\
alloc_malloc_aligned_e(sizeof(*(ptr)) * (size), alignment)


attr_inline attr_alloc_fn void*
alloc_malloc_e(
	alloc_t size
	)
{
	return alloc_alloc_e(size, 0);
}


#define alloc_malloc(ptr, size)	\
alloc_malloc_e(sizeof(*(ptr)) * (size))


attr_inline attr_alloc_fn void*
alloc_calloc_aligned_e(
	alloc_t size,
	alloc_t alignment
	)
{
	return alloc_alloc_aligned_e(size, alignment, 1);
}


#define alloc_calloc_aligned(ptr, size, alignment)	\
alloc_calloc_aligned_e(sizeof(*(ptr)) * (size), alignment)


attr_inline attr_alloc_fn void*
alloc_calloc_e(
	alloc_t size
	)
{
	return alloc_alloc_e(size, 1);
}


#define alloc_calloc(ptr, size)	\
alloc_calloc_e(sizeof(*(ptr)) * (size))


attr_inline void*
alloc_remalloc_aligned_e(
	const volatile void* ptr,
	alloc_t old_size,
	alloc_t old_alignment,
	alloc_t new_size,
	alloc_t new_alignment
	)
{
	return alloc_realloc_aligned_e(ptr, old_size, old_alignment, new_size, new_alignment, 0);
}


#define alloc_remalloc_aligned(ptr, old_size, old_alignment, new_size, new_alignment)	\
alloc_remalloc_aligned_e(ptr, sizeof(*(ptr)) * (old_size), old_alignment, sizeof(*(ptr)) * (new_size), new_alignment)


attr_inline void*
alloc_remalloc_e(
	const volatile void* ptr,
	alloc_t old_size,
	alloc_t new_size
	)
{
	return alloc_realloc_e(ptr, old_size, new_size, 0);
}


#define alloc_remalloc(ptr, old_size, new_size)	\
alloc_remalloc_e(ptr, sizeof(*(ptr)) * (old_size), sizeof(*(ptr)) * (new_size))


attr_inline void*
alloc_recalloc_aligned_e(
	const volatile void* ptr,
	alloc_t old_size,
	alloc_t old_alignment,
	alloc_t new_size,
	alloc_t new_alignment
	)
{
	return alloc_realloc_aligned_e(ptr, old_size, old_alignment, new_size, new_alignment, 1);
}


#define alloc_recalloc_aligned(ptr, old_size, old_alignment, new_size, new_alignment)	\
alloc_recalloc_aligned_e(ptr, sizeof(*(ptr)) * (old_size), old_alignment, sizeof(*(ptr)) * (new_size), new_alignment)


attr_inline void*
alloc_recalloc_e(
	const volatile void* ptr,
	alloc_t old_size,
	alloc_t new_size
	)
{
	return alloc_realloc_e(ptr, old_size, new_size, 1);
}


#define alloc_recalloc(ptr, old_size, new_size)	\
alloc_recalloc_e(ptr, sizeof(*(ptr)) * (old_size), sizeof(*(ptr)) * (new_size))


#ifndef ALLOC_RELEASE


	extern attr_api void
	alloc_check_red_zones_e(
		volatile const void* ptr,
		alloc_t size
		);


	#define alloc_check_red_zones(ptr, size)	\
	alloc_check_red_zones_e(ptr, sizeof(*(ptr)) * (size))

#else
	#define alloc_check_red_zones_e(ptr, size)
	#define alloc_check_red_zones(ptr, size)
#endif

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
#include <alloc/types.h>
#include <alloc/atomic.h>

#include <stdint.h>


extern uint64_t
alloc_huge_read_tsc(
	void
	);


extern void
alloc_huge_init(
	alloc_numa_local_data_t* local
	);


extern void
alloc_huge_global_init(
	void
	);


extern attr_cold_fn void
alloc_huge_reap_due(
	uint16_t numa,
	uint64_t now_tsc
	);


extern attr_cold_fn attr_alloc_fn void*
alloc_huge_alloc(
	alloc_t size,
	int zero,
	uint16_t numa
	);


extern attr_cold_fn void
alloc_huge_free(
	const volatile void* ptr,
	alloc_t size,
	uint16_t numa
	);


/* GCC doesn't want to inline it if it's a normal function in huge.c */
attr_inline void
alloc_huge_maintenance(
	uint16_t numa,
	uint64_t _Atomic* timer
	)
{
	uint64_t now_tsc = alloc_huge_read_tsc();

	if(attr_unlikely(now_tsc >= atomic_load_rx(timer)))
	{
		alloc_huge_reap_due(numa, now_tsc);
	}
}

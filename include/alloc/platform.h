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

#include <stdint.h>


extern const char**
alloc_get_env_array(
	void
	);


extern const char*
alloc_get_env(
	const char* name
	);


extern alloc_t
alloc_read_page_size(
	void
	);


extern alloc_t
alloc_read_huge_page_size(
	void
	);


extern uint32_t
alloc_get_random(
	void
	);


extern uint16_t
alloc_get_cpus(
	void
	);


extern uint64_t
alloc_read_time_ns(
	void
	);


extern uint16_t
alloc_get_numa_nodes(
	void
	);


extern void
alloc_get_cpu_numa_node(
	uint16_t* cpu,
	uint16_t* numa
	);


extern bool
alloc_is_numa_node_valid(
	uint16_t numa
	);


extern bool
alloc_is_tty(
	int fd
	);


extern attr_alloc_fn attr_noinline void*
alloc_alloc_virtual_e(
	alloc_t reserve_size,
	alloc_t commit_size,
	uint32_t numa,
	int thp
	);


#define alloc_alloc_virtual(ptr, reserve_size, commit_size, numa, thp)					\
alloc_alloc_virtual_e(sizeof(*(ptr)) * (reserve_size),									\
__builtin_choose_expr((commit_size) == -1, -1, sizeof(*(ptr)) * (commit_size)), (numa), (thp))


extern attr_noinline void
alloc_free_virtual_e(
	const volatile void* ptr,
	alloc_t size
	);


#define alloc_free_virtual(ptr, size)	\
alloc_free_virtual_e((ptr), sizeof(*(ptr)) * (size))


extern attr_noinline void*
alloc_realloc_virtual_e(
	const volatile void* ptr,
	alloc_t old_size,
	alloc_t new_size
	);


#define alloc_realloc_virtual(ptr, old_size, new_size)	\
alloc_realloc_virtual_e((ptr), sizeof(*(ptr)) * (old_size), sizeof(*(ptr)) * (new_size))


extern bool
alloc_commit_virtual_e(
	const volatile void* ptr,
	alloc_t size
	);


#define alloc_commit_virtual(ptr, size)	\
alloc_commit_virtual_e((ptr), sizeof(*(ptr)) * (size))


extern void
alloc_decommit_virtual_e(
	const volatile void* ptr,
	alloc_t size
	);


#define alloc_decommit_virtual(ptr, size)	\
alloc_decommit_virtual_e((ptr), sizeof(*(ptr)) * (size))


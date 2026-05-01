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

#include <alloc/sync.h>

#include <stdint.h>

#if defined(NDEBUG) && !defined(ALLOC_DEBUG)
	#define ALLOC_RELEASE
#endif

#ifdef ALLOC_RELEASE
	#define ALLOC_RED_ZONE_SIZE 0
#elif !defined(ALLOC_RED_ZONE_SIZE)
	#define ALLOC_RED_ZONE_SIZE sizeof(void*)
#endif

#ifndef ALLOC_CONFIG_NAME
	#define ALLOC_CONFIG_NAME "ALLOC_CONFIG"
#endif

#if __SIZEOF_POINTER__ == 4
	#define ALLOC_MAX_HANDLE_COUNT 40
#else
	#define ALLOC_MAX_HANDLE_COUNT 39
#endif

#define ALLOC_MAX_UNKNOWN_PARAMS 8
#define ALLOC_MAX_NUMA_NODES 64
#define ALLOC_MAX_SLAB_CLASSES 16
#define ALLOC_MAX_ARENA_FREE_PER_THREAD 128
#define ALLOC_MAX_TCACHE_CAPACITY 64
#define ALLOC_MAX_TCACHE_STACK_CAPACITY (ALLOC_MAX_TCACHE_CAPACITY * 2)
#define ALLOC_MAX_REPORT_THREADS 1024
#define ALLOC_REPORT_HUGE_SLOTS 1024
#define ALLOC_LOG_TAG_DEFAULT "[alloc]"


typedef uintptr_t alloc_t;
typedef uint8_t alloc_numa_t;
typedef uint16_t alloc_tid_idx_t;
typedef uint32_t alloc_tid_t;
typedef uint32_t alloc_handle_idx_t;
typedef uint32_t alloc_arena_idx_t;
typedef uint8_t alloc_slab_idx_t;
typedef uint16_t alloc_block_idx_t;


#define alloc_tid_numa(tid) ((alloc_numa_t)((tid) & 0xFF))
#define alloc_tid_idx(tid) ((alloc_tid_idx_t)((tid) >> (sizeof(alloc_numa_t) * 8)))
#define alloc_make_tid(idx, numa) (((alloc_tid_t)(idx) << (sizeof(alloc_numa_t) * 8)) | (alloc_numa_t)(numa))
#define ALLOC_DEAD_THREAD_TID ((alloc_tid_t) -1)


typedef struct alloc_tcb alloc_tcb_t;
typedef struct alloc_handle alloc_handle_t;
typedef struct alloc_slab_header alloc_slab_header_t;
typedef struct alloc_huge_node alloc_huge_node_t;


typedef struct alloc_arena_header alloc_arena_header_t;

struct alloc_arena_header
{
	alloc_tcb_t* tcb;
	void* data;

	alloc_arena_header_t* next;
	alloc_arena_header_t* prev;

	alloc_arena_header_t* link_next;
	alloc_arena_header_t* link_prev;

	alloc_tid_t _Atomic tid;
	alloc_arena_idx_t pool_idx;
	alloc_arena_idx_t local_idx;
	uint16_t numa;
	alloc_slab_idx_t used;
	alloc_slab_idx_t count;
	uint8_t in_empty;
	uint8_t clean;
	uint8_t free;

	uint32_t free_base_blocks;
	uint32_t btree_avail_mask;
	uint32_t vacant_mask;
	uint32_t _Atomic zombie_blocks;
};


struct alloc_slab_header
{
	alloc_handle_t* handle;

	alloc_slab_header_t* next;
	alloc_slab_header_t* prev;

	alloc_arena_header_t* arena;
	void* free;
	void* used;

	alloc_t alloc_size;
	alloc_block_idx_t count;
	alloc_block_idx_t alloc_limit;
	alloc_slab_idx_t idx;
	alloc_slab_idx_t span;
	uint8_t clean;
	uint8_t in_empty_cache;

	void* slab_data;
	alloc_slab_header_t* foreign_next;
	void* _Atomic foreign_free;
};


struct alloc_handle
{
	alloc_t slab_mask;
	uint32_t slab_shift;
	uint32_t padding;
	alloc_block_idx_t alloc_limit;
	alloc_block_idx_t empty_slabs;
	uint8_t slab_class;

	alloc_t alloc_size;
	alloc_t alignment;

	alloc_slab_header_t* slab;
	alloc_slab_header_t* _Atomic foreign_slab;

	void* next_page;
};


struct alloc_tcb
{
	uint32_t _Atomic ref_count;
	alloc_tid_t tid;

	alloc_tcb_t* next_free;

	alloc_handle_t handles[ALLOC_MAX_HANDLE_COUNT];
};


struct alloc_huge_node
{
	alloc_huge_node_t* next;
	alloc_huge_node_t* time_prev;
	alloc_huge_node_t* time_next;
	void* ptr;
	alloc_t size;
	uint64_t time_of_free;
};


typedef struct alloc_numa_local_data
{
	alloc_numa_t numa;

	struct
	{
		sync_mtx_t mtx;
		alloc_t _Atomic exhausted_events;
		alloc_t pool_region_size;
		void* pool_region;
		void* pool_first_slab;
		alloc_t slot_count;
		alloc_t used_slots;
		alloc_t committed_slots;
		alloc_t prefix_committed_slots;
		alloc_t high_watermark;
		alloc_t prefix_high_watermark;
		alloc_arena_header_t* link_tail;
		uint64_t* bitmap_l0;
		uint64_t* bitmap_l1;
	}
	arena;

	struct
	{
		sync_mtx_t mtx;
		uint64_t _Atomic timer;
		alloc_huge_node_t* free_node;
		alloc_huge_node_t* time_head;
		alloc_huge_node_t* time_tail;
		alloc_huge_node_t* nodes;
		alloc_huge_node_t** ht;
	}
	huge;

	struct
	{
		sync_mtx_t mtx;
		alloc_t count;
		alloc_t committed;
		alloc_tcb_t* free;
		alloc_tcb_t* ptr;
	}
	tcb;
}
alloc_numa_local_data_t;

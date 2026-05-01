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

#include <alloc/log.h>
#include <alloc/threads.h>


typedef struct alloc_handle_info
{
	alloc_t alloc_size;
	alloc_t alignment;
}
alloc_handle_info_t;


typedef enum alloc_consts_state
{
	ALLOC_CONSTS_STATE_UNINITIALIZED	= 0,
	ALLOC_CONSTS_STATE_BOOTSTRAP		= 1,
	ALLOC_CONSTS_STATE_INITIALIZED		= 2,
	MACRO_ENUM_BITS(ALLOC_CONSTS_STATE)
}
alloc_consts_state_t;


typedef struct attr_aligned(4096) alloc_consts
{
	alloc_consts_state_t _Atomic state;

	struct
	{
		alloc_t size;
		alloc_t random;
	}
	red_zone;

	struct
	{
		alloc_log_verbosity_t verbosity;
		alloc_t fd;
		alloc_t dump_consts;
	}
	log;

	struct
	{
		alloc_t fd;
		alloc_t period_ms;
		alloc_t enable;
		alloc_t per_thread_enable;
	}
	report;

	struct
	{
		alloc_t size;
		alloc_t reserve_size;
	}
	bootstrap;

	struct
	{
		alloc_t calibrate_ms;
		alloc_t calibrate_ns;
		alloc_t threshold_ms;
		alloc_t threshold_ns;
		alloc_t slots;
		alloc_t slots_mask;
		alloc_t slots_shift;
	}
	huge;

	struct
	{
		alloc_t size;
		alloc_t mask;
		alloc_t shift;
	}
	page;

	struct
	{
		alloc_t size;
		alloc_t mask;
		alloc_t shift;
	}
	huge_page;

	struct
	{
		alloc_t size;
		alloc_t mask;
		alloc_t shift;
		alloc_t virtual_capacity;
		alloc_t max_free_per_thread;
		alloc_t preallocate;
		alloc_t commit_batch;
		alloc_t thp_enable;
		alloc_t align;
		alloc_t tail_decommit_enable;
		alloc_t tail_decommit_trigger_div;
		alloc_t tail_decommit_amount_div;
		alloc_t prefix_thp_enable;
		alloc_t prefix_tail_decommit_enable;
		alloc_t prefix_tail_decommit_trigger_div;
		alloc_t prefix_tail_decommit_amount_div;
		alloc_t btree_offset;
		alloc_t vacant_next_offset;
		alloc_t vacant_prev_offset;
		alloc_t slab_indexable_offset;
		alloc_t prefix_size;
		alloc_t data_region_size;
		alloc_t data_region_mask;
		alloc_t prefix_region_offset;
		alloc_t data_offset;
		alloc_t slab_count;
		alloc_t prefix_slabs;
		uint64_t btree_template[ALLOC_MAX_SLAB_CLASSES];
		uint32_t btree_avail_mask_template;
	}
	arena;

	struct
	{
		alloc_t small;
		alloc_t small_shift;
		alloc_t medium;
		alloc_t medium_shift;
		alloc_t large;
		alloc_t large_shift;
		alloc_t class_count;
		alloc_t class_span[ALLOC_MAX_SLAB_CLASSES];
		alloc_t class_span_shift[ALLOC_MAX_SLAB_CLASSES];
		alloc_t small_min_blocks;
		alloc_t small_min_blocks_shift;
		alloc_t medium_min_blocks;
		alloc_t medium_min_blocks_shift;
		alloc_t large_min_blocks;
		alloc_t large_min_blocks_shift;
		alloc_t virtual_cutoff;
		alloc_t virtual_cutoff_shift;
		alloc_t max_nonvirtual_size;
		alloc_t max_free_per_handle;
		alloc_t smallest_allocation_shift;
	}
	slab;

	struct
	{
		alloc_handle_t templates[ALLOC_MAX_HANDLE_COUNT];
		alloc_handle_idx_t count;
	}
	handles;

	struct
	{
		alloc_t max_per_class;
		alloc_t capacity;
		alloc_t mask;
	}
	tcache;

	struct
	{
		alloc_t capacity;
		alloc_t preallocate;
		alloc_t commit_batch;
	}
	tcb;

	struct
	{
		alloc_t arena_bitmap_l0_size;
		alloc_t arena_bitmap_l0_offset;
		alloc_t arena_bitmap_l1_size;
		alloc_t arena_bitmap_l1_offset;
		alloc_t huge_nodes_offset;
		alloc_t huge_ht_offset;
		alloc_t tcb_size;
		alloc_t tcb_preallocate_size;
		alloc_t tcb_offset;

		alloc_t data_size;
		alloc_t preallocated_size;

		uint16_t nodes;
		alloc_numa_t valid;
		alloc_numa_t map[ALLOC_MAX_NUMA_NODES];
		alloc_numa_t reverse_map[ALLOC_MAX_NUMA_NODES];

		union
		{
			alloc_numa_local_data_t* single;
			alloc_numa_local_data_t** multiple;
		}
		data;
	}
	numa;

	struct
	{
		thread_key_t key;
	}
	thread;

#ifndef ALLOC_RELEASE
	alloc_t _Atomic first_use_done;
#endif
}
alloc_consts_t;


extern alloc_consts_t alloc_consts;


extern void
alloc_thread_dtor_fn(
	void* arg
	);


typedef struct alloc_config
{
	alloc_t red_zone_size;
	alloc_t log_verbosity;
	alloc_t log_fd;
	alloc_t log_dump_consts;
	alloc_t report_fd;
	alloc_t report_period_ms;
	alloc_t report_enable;
	alloc_t report_per_thread_enable;
	alloc_t huge_threshold_ms;
	alloc_t arena_max_free_per_thread;
	alloc_t arena_commit_batch;
	alloc_t arena_tail_decommit_enable;
	alloc_t arena_tail_decommit_trigger_div;
	alloc_t arena_tail_decommit_amount_div;
	alloc_t arena_prefix_tail_decommit_enable;
	alloc_t arena_prefix_tail_decommit_trigger_div;
	alloc_t arena_prefix_tail_decommit_amount_div;
	alloc_t slab_small;
	alloc_t slab_medium;
	alloc_t slab_large;
	alloc_t slab_small_min_blocks;
	alloc_t slab_medium_min_blocks;
	alloc_t slab_large_min_blocks;
	alloc_t slab_max_free_per_handle;
	alloc_t tcache_max_per_class;
	alloc_t tcb_commit_batch;

	alloc_t red_zone_size_set:1;
	alloc_t log_verbosity_set:1;
	alloc_t log_fd_set:1;
	alloc_t log_dump_consts_set:1;
	alloc_t report_fd_set:1;
	alloc_t report_period_ms_set:1;
	alloc_t report_enable_set:1;
	alloc_t report_per_thread_enable_set:1;
	alloc_t huge_threshold_ms_set:1;
	alloc_t arena_max_free_per_thread_set:1;
	alloc_t arena_commit_batch_set:1;
	alloc_t arena_tail_decommit_enable_set:1;
	alloc_t arena_tail_decommit_trigger_div_set:1;
	alloc_t arena_tail_decommit_amount_div_set:1;
	alloc_t arena_prefix_tail_decommit_enable_set:1;
	alloc_t arena_prefix_tail_decommit_trigger_div_set:1;
	alloc_t arena_prefix_tail_decommit_amount_div_set:1;
	alloc_t slab_small_set:1;
	alloc_t slab_medium_set:1;
	alloc_t slab_large_set:1;
	alloc_t slab_small_min_blocks_set:1;
	alloc_t slab_medium_min_blocks_set:1;
	alloc_t slab_large_min_blocks_set:1;
	alloc_t slab_max_free_per_handle_set:1;
	alloc_t tcache_max_per_class_set:1;
	alloc_t tcb_commit_batch_set:1;
}
alloc_config_t;


extern void
alloc_init(
	void
	);


extern void
alloc_configure(
	const alloc_config_t* config
	);


extern alloc_numa_local_data_t*
alloc_get_numa_local_data(
	alloc_numa_t numa
	);


extern alloc_numa_t
alloc_numa_to_logical(
	uint16_t numa
	);


extern uint16_t
alloc_numa_to_physical(
	alloc_numa_t numa
	);

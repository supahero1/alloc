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
#include <alloc/tcb.h>
#include <alloc/attr.h>
#include <alloc/base.h>
#include <alloc/huge.h>
#include <alloc/test.h>
#include <alloc/arena.h>
#include <alloc/btree.h>
#include <alloc/debug.h>
#include <alloc/macro.h>
#include <alloc/types.h>
#include <alloc/atomic.h>
#include <alloc/consts.h>
#include <alloc/report.h>
#include <alloc/threads.h>
#include <alloc/platform.h>
#include <alloc/red_zone.h>
#include <alloc/valgrind.h>
#include <alloc/bootstrap.h>

#include <stdint.h>
#include <string.h>


typedef struct alloc_empty
{
	alloc_arena_idx_t count;
	alloc_arena_header_t* ptr[ALLOC_MAX_ARENA_FREE_PER_THREAD];
}
alloc_empty_t;


typedef struct alloc_tcache_class
{
	uint8_t count;
	void** head;
	void** tail;
	void* ptr[ALLOC_MAX_TCACHE_STACK_CAPACITY];
}
alloc_tcache_class_t;


typedef struct alloc_tls
{
	alloc_tcb_t* tcb;
	alloc_handle_t* handles;

	uint8_t initialized;
	alloc_tid_t tid;
	alloc_numa_t numa;
	alloc_numa_local_data_t* numa_data;
	uint64_t _Atomic* numa_data_timer;

	struct
	{
		alloc_arena_header_t** ptr;
		alloc_arena_idx_t used;
		alloc_arena_idx_t size;
		alloc_arena_header_t* vacant_head[ALLOC_MAX_SLAB_CLASSES];
		alloc_empty_t empty;
	}
	arenas;

	struct
	{
		alloc_tcache_class_t classes[ALLOC_MAX_HANDLE_COUNT];
	}
	tcache;
}
alloc_tls_t;


thread_local alloc_tls_t alloc_tls;


void
alloc_reset(
	void
	)
{
	alloc_tls = (alloc_tls_t){0};

	alloc_init();
}


attr_cold_fn void
alloc_tls_init(
	void
	)
{
	alloc_tls.initialized = 1;

	uint16_t numa;
	alloc_get_cpu_numa_node(NULL, &numa);
	alloc_tls.numa = alloc_numa_to_logical(numa);
	alloc_tls.numa_data = alloc_get_numa_local_data(alloc_tls.numa);
	alloc_tls.numa_data_timer = &alloc_tls.numa_data->huge.timer;

	alloc_tls.tcb = alloc_tcb_acquire(alloc_tls.numa);
	alloc_tls.tid = alloc_tls.tcb->tid;
	alloc_tcb_init(alloc_tls.tcb);
	alloc_tls.handles = alloc_tls.tcb->handles;

	for(alloc_handle_idx_t i = 0; i < ALLOC_MAX_HANDLE_COUNT; ++i)
	{
		alloc_tls.tcache.classes[i].head = alloc_tls.tcache.classes[i].ptr;
		alloc_tls.tcache.classes[i].tail = alloc_tls.tcache.classes[i].ptr;
	}

	thread_key_set(alloc_consts.thread.key, &alloc_tls);

	alloc_log_debug("thread_ctor(): tid=", alloc_tls.tid, " numa=", alloc_tls.numa);
}


void
alloc_ensure_tls(
	void
	)
{
	if(attr_unlikely(!alloc_tls.initialized))
	{
		alloc_tls_init();
	}
}


void
alloc_thread_init(
	void
	)
{
	alloc_ensure_tls();
}


void
alloc_update_numa(
	void
	)
{
	alloc_ensure_tls();

	uint16_t numa;
	alloc_get_cpu_numa_node(NULL, &numa);
	alloc_tls.numa = alloc_numa_to_logical(numa);
	alloc_tls.numa_data = alloc_get_numa_local_data(alloc_tls.numa);
	alloc_tls.numa_data_timer = &alloc_tls.numa_data->huge.timer;
}


alloc_arena_header_t**
alloc_get_vacant_next_slot(
	alloc_arena_header_t* arena,
	uint32_t slab_idx
	)
{
	alloc_arena_header_t** vacant_next = (void*) arena + alloc_consts.arena.vacant_next_offset;
	return vacant_next + slab_idx;
}


alloc_arena_header_t**
alloc_get_vacant_prev_slot(
	alloc_arena_header_t* arena,
	uint32_t slab_idx
	)
{
	alloc_arena_header_t** vacant_prev = (void*) arena + alloc_consts.arena.vacant_prev_offset;
	return vacant_prev + slab_idx;
}


void
alloc_vacant_link_arena(
	alloc_arena_header_t* arena,
	uint32_t slab_idx
	)
{
	assert_not_null(arena);
	assert_lt(slab_idx, alloc_consts.slab.class_count);

	uint32_t bit = (uint32_t) 1 << slab_idx;
	if(arena->vacant_mask & bit)
	{
		return;
	}

	alloc_arena_header_t** prev_slot = alloc_get_vacant_prev_slot(arena, slab_idx);
	alloc_arena_header_t** next_slot = alloc_get_vacant_next_slot(arena, slab_idx);

	alloc_arena_header_t* old_head = alloc_tls.arenas.vacant_head[slab_idx];
	*prev_slot = NULL;
	*next_slot = old_head;

	if(old_head)
	{
		*alloc_get_vacant_prev_slot(old_head, slab_idx) = arena;
	}

	alloc_tls.arenas.vacant_head[slab_idx] = arena;
	arena->vacant_mask |= bit;
}


void
alloc_vacant_unlink_arena(
	alloc_arena_header_t* arena,
	uint32_t slab_idx
	)
{
	assert_not_null(arena);
	assert_lt(slab_idx, alloc_consts.slab.class_count);

	uint32_t bit = (uint32_t) 1 << slab_idx;
	if(!(arena->vacant_mask & bit))
	{
		return;
	}

	alloc_arena_header_t** prev_slot = alloc_get_vacant_prev_slot(arena, slab_idx);
	alloc_arena_header_t** next_slot = alloc_get_vacant_next_slot(arena, slab_idx);

	alloc_arena_header_t* prev = *prev_slot;
	alloc_arena_header_t* next = *next_slot;

	if(prev)
	{
		*alloc_get_vacant_next_slot(prev, slab_idx) = next;
	}
	else
	{
		alloc_tls.arenas.vacant_head[slab_idx] = next;
	}

	if(next)
	{
		*alloc_get_vacant_prev_slot(next, slab_idx) = prev;
	}

	*prev_slot = NULL;
	*next_slot = NULL;
	arena->vacant_mask &= ~bit;
}


void
alloc_ret_full_arena_slabs(
	alloc_arena_header_t* arena
	)
{
	assert_not_null(arena);

	alloc_slab_header_t* slab_base = (void*) arena + alloc_consts.arena.slab_indexable_offset;

	for(alloc_slab_idx_t slab_idx = alloc_consts.arena.prefix_slabs; slab_idx < arena->used; ++slab_idx)
	{
		alloc_slab_header_t* slab = slab_base + slab_idx;
		alloc_slab_idx_t span = MACRO_MAX(slab->span, 1);

		if(slab->idx != slab_idx + 1)
		{
			slab_idx += span - 1;
			continue;
		}

		if(!slab->count && slab->handle)
		{
			alloc_handle_t* handle = slab->handle;

			if(slab->next)
			{
				slab->next->prev = slab->prev;
			}

			if(slab->prev)
			{
				slab->prev->next = slab->next;
			}
			else if(handle->slab == slab)
			{
				handle->slab = slab->next;
			}

			slab->next = NULL;
			slab->prev = NULL;

			if(slab->in_empty_cache)
			{
				assert_neq(handle->empty_slabs--, 0);
				slab->in_empty_cache = 0;
			}
		}

		slab_idx += span - 1;
	}
}


void
alloc_vacant_unlink_all_arena(
	alloc_arena_header_t* arena
	)
{
	assert_not_null(arena);

	for(uint32_t slab_idx = 0; slab_idx < alloc_consts.slab.class_count; ++slab_idx)
	{
		alloc_vacant_unlink_arena(arena, slab_idx);
	}
}


void
alloc_empty_insert_arena(
	alloc_arena_header_t* arena
	)
{
	assert_not_null(arena);
	assert_lt(alloc_tls.arenas.empty.count, ALLOC_MAX_ARENA_FREE_PER_THREAD);

	alloc_t key = (alloc_t) arena;
	alloc_arena_idx_t idx = alloc_tls.arenas.empty.count;

	while(idx && (alloc_t) alloc_tls.arenas.empty.ptr[idx - 1] > key)
	{
		alloc_tls.arenas.empty.ptr[idx] = alloc_tls.arenas.empty.ptr[idx - 1];
		--idx;
	}

	alloc_tls.arenas.empty.ptr[idx] = arena;
	++alloc_tls.arenas.empty.count;
}


alloc_arena_header_t*
alloc_empty_pop_lowest(
	void
	)
{
	assert_neq(alloc_tls.arenas.empty.count, 0);

	alloc_arena_header_t* arena = alloc_tls.arenas.empty.ptr[0];
	memmove(alloc_tls.arenas.empty.ptr, alloc_tls.arenas.empty.ptr + 1, sizeof(*alloc_tls.arenas.empty.ptr) * --alloc_tls.arenas.empty.count);

	return arena;
}


void
alloc_ret_local_arena(
	alloc_arena_header_t* arena
	)
{
	assert_not_null(arena);
	assert_eq(arena->count, 0);

	alloc_ret_full_arena_slabs(arena);
	alloc_vacant_unlink_all_arena(arena);

	alloc_t max_free = alloc_consts.arena.max_free_per_thread;
	assert_le(max_free, ALLOC_MAX_ARENA_FREE_PER_THREAD);

	if(alloc_tls.arenas.empty.count < max_free)
	{
		alloc_empty_insert_arena(arena);
		arena->in_empty = 1;
		return;
	}

	if(alloc_tls.arenas.empty.count)
	{
		alloc_arena_header_t* highest_cached = alloc_tls.arenas.empty.ptr[alloc_tls.arenas.empty.count - 1];
		if((alloc_t) highest_cached > (alloc_t) arena)
		{
			--alloc_tls.arenas.empty.count;
			highest_cached->in_empty = 0;

			alloc_empty_insert_arena(arena);
			arena->in_empty = 1;

			arena = highest_cached;
		}
	}

	alloc_arena_idx_t freed_idx = arena->local_idx;
	alloc_arena_idx_t last_idx = --alloc_tls.arenas.used;

	if(freed_idx != last_idx)
	{
		alloc_arena_header_t* moved = alloc_tls.arenas.ptr[last_idx];
		alloc_tls.arenas.ptr[freed_idx] = moved;
		moved->local_idx = freed_idx;
	}

	alloc_ret_arena(arena);
}


void
alloc_vacant_refresh_arena(
	alloc_arena_header_t* arena,
	uint32_t changed_mask
	)
{
	assert_not_null(arena);

	if(!changed_mask)
	{
		return;
	}

	uint32_t highest_changed = MACRO_FLOOR_LOG2(changed_mask);
	uint32_t refresh_count = highest_changed + 1;
	assert_le(refresh_count, alloc_consts.slab.class_count);

	for(uint32_t slab_idx = 0; slab_idx < refresh_count; ++slab_idx)
	{
		int has_capacity = (arena->btree_avail_mask >> slab_idx) != 0;
		int linked = !!(arena->vacant_mask & (uint32_t) 1 << slab_idx);

		if(has_capacity && !linked)
		{
			alloc_vacant_link_arena(arena, slab_idx);
		}
		else if(!has_capacity && linked)
		{
			alloc_vacant_unlink_arena(arena, slab_idx);
		}
	}
}


uint64_t*
alloc_get_arena_btree(
	alloc_arena_header_t* arena
	)
{
	return (void*) arena + alloc_consts.arena.btree_offset;
}


void
alloc_handle_ret_empty_slab(
	alloc_handle_t* handle,
	alloc_slab_header_t* slab
	)
{
	assert_not_null(handle);
	assert_not_null(slab);

	assert_eq(slab->count, 0);
	assert_neq(slab->span, 0);
	assert_true(slab->in_empty_cache);
	slab->in_empty_cache = 0;
	assert_neq(handle->empty_slabs--, 0);

	alloc_arena_header_t* arena = slab->arena;
	alloc_t slab_class_idx = handle->slab_class;

	alloc_slab_header_t* next = slab->next;
	alloc_slab_header_t* prev = slab->prev;

	if(next)
	{
		next->prev = prev;
	}

	if(prev)
	{
		prev->next = next;
	}
	else if(handle->slab == slab)
	{
		handle->slab = next;
	}

	slab->next = NULL;
	slab->prev = NULL;

	uint32_t changed_mask = 0;
	alloc_btree_ret(alloc_get_arena_btree(arena), slab_class_idx, slab->idx - 1, &arena->btree_avail_mask, &changed_mask);
	arena->free_base_blocks += slab->span;

	if(attr_unlikely(!arena->count && arena->free_base_blocks == alloc_consts.arena.slab_count))
	{
		alloc_ret_local_arena(arena);
		return;
	}

	alloc_vacant_refresh_arena(arena, changed_mask);
}


bool
alloc_is_size_virtual(
	alloc_t size
	)
{
	return size > alloc_consts.slab.max_nonvirtual_size;
}


alloc_handle_idx_t
alloc_default_idx_fn(
	alloc_t size
	)
{
	size = MACRO_MAX(size, sizeof(void*));
	return MACRO_CEIL_LOG2(size) - alloc_consts.slab.smallest_allocation_shift;
}


attr_pure_fn alloc_handle_idx_t
alloc_get_handle_idx(
	alloc_t size
	)
{
	assert_neq(size, 0);
	assert_false(alloc_is_size_virtual(size));

	return alloc_default_idx_fn(size);
}


attr_pure_fn const alloc_handle_t*
alloc_get_default_handle(
	alloc_t size
	)
{
	alloc_ensure_tls();

	return alloc_tls.handles + alloc_get_handle_idx(size);
}


attr_inline void**
alloc_tcache_slot(
	alloc_tcache_class_t* class,
	alloc_t idx
	)
{
	return class->ptr + (idx & alloc_consts.tcache.mask);
}


alloc_arena_header_t*
alloc_ptr_to_arena(
	const volatile void* ptr
	)
{
	assert_not_null(ptr);

	void* data_region_base = (void*) MACRO_ALIGN_DOWN(ptr, alloc_consts.arena.data_region_mask);
	alloc_t slot = ((void*) ptr - data_region_base) >> alloc_consts.arena.shift;
	assert_lt(slot, alloc_consts.arena.virtual_capacity);

	void* prefix_base = data_region_base + alloc_consts.arena.prefix_region_offset;
	return prefix_base + slot * alloc_consts.arena.prefix_size;
}


void*
alloc_tcache_try_pop_handle(
	alloc_handle_idx_t handle_idx,
	const alloc_handle_t* handle,
	alloc_t requested_size,
	int zero
	)
{
	assert_not_null(handle);

	alloc_tcache_class_t* class = alloc_tls.tcache.classes + handle_idx;
	uint8_t count = class->count;
	if(!count)
	{
		return NULL;
	}

	alloc_t tail_idx = class->tail - class->ptr;
	class->tail = alloc_tcache_slot(class, tail_idx - 1);
	void* ptr = *class->tail;
	class->count = count - 1;

	if(zero)
	{
		memset(ptr, 0, requested_size);
	}

	return ptr;
}


void
alloc_free_own_internal(
	void* ptr,
	alloc_arena_header_t* arena,
	alloc_slab_header_t* slab
	);


attr_noinline void
alloc_tcache_flush_ptr(
	void* flush_ptr,
	const alloc_handle_t* handle
	)
{
	alloc_arena_header_t* flush_arena = alloc_ptr_to_arena(flush_ptr);
	alloc_slab_idx_t flush_slab_idx = (((void*) flush_ptr - flush_arena->data) & handle->slab_mask)
		>> alloc_consts.slab.small_shift;
	alloc_slab_header_t* flush_slab_base = (void*) flush_arena + alloc_consts.arena.slab_indexable_offset;
	alloc_slab_header_t* flush_slab = flush_slab_base + flush_slab_idx;

	alloc_free_own_internal(flush_ptr, flush_arena, flush_slab);
}


bool
alloc_tcache_try_push_handle(
	alloc_handle_idx_t handle_idx,
	const alloc_handle_t* handle,
	void* ptr
	)
{
	assert_not_null(handle);
	assert_not_null(ptr);

	if(attr_unlikely(!alloc_consts.tcache.capacity))
	{
		return false;
	}

	alloc_tcache_class_t* class = alloc_tls.tcache.classes + handle_idx;
	alloc_t count = class->count;

	if(attr_unlikely(count >= alloc_consts.tcache.capacity))
	{
		for(alloc_t i = 0; i < alloc_consts.tcache.max_per_class; ++i)
		{
			void* flush_ptr = *class->head;
			class->head = alloc_tcache_slot(class, class->head - class->ptr + 1);

			alloc_tcache_flush_ptr(flush_ptr, handle);
		}

		count -= alloc_consts.tcache.max_per_class;
	}

	*class->tail = ptr;
	class->tail = alloc_tcache_slot(class, class->tail - class->ptr + 1);
	class->count = count + 1;

	return true;
}


alloc_arena_header_t*
alloc_get_new_local_arena(
	void
	)
{
	if(attr_unlikely(alloc_tls.arenas.used >= alloc_tls.arenas.size))
	{
		alloc_arena_idx_t new_size = (alloc_tls.arenas.size << 1) | 3;
		assert_neq(new_size, alloc_tls.arenas.size);

		alloc_arena_header_t** new_ptr = alloc_realloc_virtual(alloc_tls.arenas.ptr, alloc_tls.arenas.size, new_size);
		assert_not_null(new_ptr);

		alloc_tls.arenas.ptr = new_ptr;
		alloc_tls.arenas.size = new_size;
	}

	alloc_arena_header_t* arena = alloc_get_arena(alloc_tls.tcb, alloc_tls.numa);
	if(attr_unlikely(!arena))
	{
		return NULL;
	}

	alloc_arena_idx_t arena_idx = alloc_tls.arenas.used++;
	alloc_tls.arenas.ptr[arena_idx] = arena;
	arena->local_idx = arena_idx;

	return arena;
}


alloc_arena_header_t*
alloc_get_local_arena(
	uint32_t slab_class_idx
	)
{
	assert_lt(slab_class_idx, alloc_consts.slab.class_count);

	alloc_arena_header_t* head = alloc_tls.arenas.vacant_head[slab_class_idx];
	if(attr_likely(head))
	{
		return head;
	}

	if(attr_likely(alloc_tls.arenas.empty.count))
	{
		alloc_arena_header_t* arena = alloc_empty_pop_lowest();
		arena->in_empty = 0;
		arena->used = 0;
		arena->vacant_mask = 0;
		atomic_store_rx(&arena->zombie_blocks, 0);

		alloc_slab_header_t* slab_base = (void*) arena + alloc_consts.arena.slab_indexable_offset;
		memset(slab_base + alloc_consts.arena.prefix_slabs, 0, sizeof(alloc_slab_header_t) * alloc_consts.arena.slab_count);
		memcpy(alloc_get_arena_btree(arena), alloc_consts.arena.btree_template, alloc_btree_size());
		arena->btree_avail_mask = alloc_consts.arena.btree_avail_mask_template;
		arena->free_base_blocks = (uint32_t) alloc_consts.arena.slab_count;
		arena->clean = 0;
		alloc_vacant_refresh_arena(arena, arena->btree_avail_mask);

		return arena;
	}

	alloc_arena_header_t* arena = alloc_get_new_local_arena();
	if(arena)
	{
		alloc_vacant_refresh_arena(arena, arena->btree_avail_mask);
	}

	return arena;
}


attr_noinline void*
alloc_get_slab(
	alloc_handle_t* handle
	)
{
	if(attr_unlikely(atomic_load_rx(&handle->foreign_slab)))
	{
		alloc_slab_header_t* foreign_head;
		while(1)
		{
			foreign_head = atomic_load_acq(&handle->foreign_slab);

			if(atomic_exchange_weak_acq_rel(&handle->foreign_slab, &foreign_head, foreign_head->foreign_next))
			{
				break;
			}
		}

		alloc_slab_header_t* f_slab = foreign_head;
		alloc_arena_header_t* f_arena = f_slab->arena;
		alloc_block_idx_t f_slab_active = f_slab->count;
		alloc_block_idx_t f_foreign_seen = 0;
		alloc_t f_slab_size = (alloc_t) 1 << f_slab->handle->slab_shift;
		void* f_slab_begin = f_slab->slab_data;
		void* f_slab_end = f_slab_begin + f_slab_size;

		void* block = atomic_exchange_acq(&f_slab->foreign_free, NULL);
		while(block)
		{
			assert_neq(block, (void*) -1, alloc_log_error("get_slab(): malformed foreign list sentinel, arena=", f_arena->local_idx,
				" slab_idx=", f_slab->idx, " tid=", f_arena->tid));

			assert_true(block >= f_slab_begin && block < f_slab_end,
				alloc_log_error("get_slab(): malformed foreign block ptr=", block,
				" arena=", f_arena->local_idx, " slab_idx=", f_slab->idx, " tid=", f_arena->tid,
				" slab_begin=", f_slab_begin, " slab_end=", f_slab_end));

			assert_true(++f_foreign_seen <= f_slab_active,
				alloc_log_error("get_slab(): foreign list longer than active count, arena=", f_arena->local_idx,
				" slab_idx=", f_slab->idx, " active=", f_slab_active, " seen=", f_foreign_seen));

			void* next_block;
			memcpy(&next_block, block, sizeof(next_block));

			memcpy(block, &f_slab->free, sizeof(f_slab->free));
			f_slab->free = block;

			if(attr_unlikely(!--f_slab->count))
			{
				--f_arena->count;

				f_slab->free = NULL;
				f_slab->used = f_slab->slab_data;
				f_slab->clean = 0;
				f_slab->in_empty_cache = 1;

				if(attr_unlikely(f_slab->alloc_limit == 1))
				{
					if(handle->slab)
					{
						handle->slab->prev = f_slab;
					}

					f_slab->next = handle->slab;
					f_slab->prev = NULL;
					handle->slab = f_slab;
				}

				if(++handle->empty_slabs > alloc_consts.slab.max_free_per_handle)
				{
					alloc_handle_ret_empty_slab(handle, f_slab);
				}
			}
			else if(attr_unlikely(f_slab->count == f_slab->alloc_limit - 1))
			{
				if(handle->slab)
				{
					handle->slab->prev = f_slab;
					f_slab->next = handle->slab;
				}
				else
				{
					f_slab->next = NULL;
				}

				f_slab->prev = NULL;
				handle->slab = f_slab;
			}

			block = next_block;
		}
	}

	if(attr_likely(!handle->slab))
	{
		alloc_arena_header_t* arena = alloc_get_local_arena(handle->slab_class);
		if(attr_unlikely(!arena))
		{
			return NULL;
		}

		alloc_slab_header_t* slab_base = (void*) arena + alloc_consts.arena.slab_indexable_offset;

		alloc_t base_span = alloc_consts.slab.class_span[handle->slab_class];
		uint32_t changed_mask = 0;
		alloc_t arena_slab_count = alloc_consts.arena.slab_count;

		alloc_slab_idx_t slab_idx = alloc_btree_get(alloc_get_arena_btree(arena),
			handle->slab_class, &arena->btree_avail_mask, &changed_mask);
		assert_eq(slab_idx & (base_span - 1), 0);
		assert_le(slab_idx + base_span, arena_slab_count);
		assert_ge(arena->free_base_blocks, base_span);
		arena->free_base_blocks -= base_span;

		alloc_slab_idx_t old_watermark = arena->used;
		if(arena->used < slab_idx + base_span)
		{
			arena->used = slab_idx + base_span;
		}

		alloc_slab_header_t* slab = slab_base + slab_idx;

		uint8_t slab_clean = slab_idx >= old_watermark ? arena->clean : 0;
		void* slab_data = arena->data + handle->padding + (slab_idx << alloc_consts.slab.small_shift);

		*slab =
		(alloc_slab_header_t)
		{
			.handle = handle,
			.arena = arena,
			.used = slab_data,
			.alloc_size = handle->alloc_size,
			.alloc_limit = handle->alloc_limit,
			.idx = slab_idx + 1,
			.span = base_span,
			.clean = slab_clean
		};

		slab->slab_data = slab_data;
		slab->foreign_next = NULL;
		atomic_store_rx(&slab->foreign_free, NULL);

		alloc_vacant_refresh_arena(arena, changed_mask);

		handle->slab = slab;
	}

	return handle->slab;
}


void*
alloc_alloc_slab_internal(
	alloc_handle_t* handle,
	alloc_t size,
	int zero
	)
{
	alloc_slab_header_t* slab;
	if(attr_likely(handle->slab))
	{
		slab = handle->slab;
	}
	else
	{
		slab = alloc_get_slab(handle);
		if(attr_unlikely(!slab))
		{
			return NULL;
		}
	}

	void* ptr;
	int should_zero = zero;

	if(slab->free)
	{
		ptr = slab->free;
		memcpy(&slab->free, ptr, sizeof(slab->free));
	}
	else
	{
		ptr = slab->used;
		slab->used += slab->alloc_size;
		should_zero &= !slab->clean;
	}

	if(attr_unlikely(!slab->count++))
	{
		if(slab->in_empty_cache)
		{
			assert_neq(handle->empty_slabs, 0);
			--handle->empty_slabs;
			slab->in_empty_cache = 0;
		}

		++slab->arena->count;
	}

	if(attr_unlikely(slab->count == slab->alloc_limit))
	{
		handle->slab = slab->next;

		if(slab->next)
		{
			slab->next->prev = NULL;
		}

		slab->next = NULL;
	}

	if(should_zero)
	{
		memset(ptr, 0, size);
	}

	return ptr;
}


attr_alloc_fn void*
alloc_alloc_e(
	alloc_t size,
	int zero
	)
{
#ifndef ALLOC_RELEASE
	atomic_store_rel(&alloc_consts.first_use_done, 1);
#endif

	if(attr_unlikely(!size))
	{
		return NULL;
	}

	if(attr_unlikely(atomic_load_acq(&alloc_consts.state) < ALLOC_CONSTS_STATE_INITIALIZED))
	{
		return alloc_bootstrap_alloc(size, zero);
	}

	alloc_ensure_tls();
	alloc_huge_maintenance(alloc_tls.numa, alloc_tls.numa_data_timer);

	if(attr_unlikely(alloc_is_size_virtual(size)))
	{
		void* ptr = alloc_huge_alloc(size, zero, alloc_tls.numa);
		ALLOC_VALGRIND_ALLOC(ptr, size, zero);
		return ptr;
	}

	alloc_log_debug("alloc(): size=", size, " zero=", zero, " tid=", alloc_tls.tid);

	alloc_handle_idx_t handle_idx = alloc_get_handle_idx(size);
	const alloc_handle_t* handle = alloc_tls.handles + handle_idx;

	void* tcache_ptr = alloc_tcache_try_pop_handle(handle_idx, handle, size, zero);
	if(tcache_ptr)
	{
		ALLOC_VALGRIND_DISABLE_ERROR_REPORTING();

		alloc_red_zone_init(tcache_ptr - alloc_consts.red_zone.size);
		alloc_red_zone_init(tcache_ptr + size);

		ALLOC_VALGRIND_ENABLE_ERROR_REPORTING();
		ALLOC_VALGRIND_ALLOC(tcache_ptr, size, zero);

		return tcache_ptr;
	}

	ALLOC_VALGRIND_DISABLE_ERROR_REPORTING();

	void* ptr = alloc_alloc_slab_internal((void*) handle, size, zero);
	if(ptr)
	{
		alloc_red_zone_init(ptr - alloc_consts.red_zone.size);
		alloc_red_zone_init(ptr + size);
	}

	ALLOC_VALGRIND_ENABLE_ERROR_REPORTING();
	ALLOC_VALGRIND_ALLOC(ptr, size, zero);

	return ptr;
}


void
alloc_free_trap(
	const volatile void* ptr,
	alloc_t size,
	const alloc_handle_t* handle
	)
{
	assert_not_null(ptr);
	assert_not_null(handle);

	alloc_log_error("free(): malformed arguments, ptr=", ptr, " size=", size,
		" handle->alloc_size=", handle->alloc_size, " handle->alignment=", handle->alignment);
}


void
alloc_free_own_internal(
	void* ptr,
	alloc_arena_header_t* arena,
	alloc_slab_header_t* slab
	)
{
	if(attr_unlikely(!--slab->count))
	{
		--arena->count;

		slab->free = NULL;
		slab->used = slab->slab_data;
		atomic_store_rx(&slab->foreign_free, NULL);
		slab->clean = 0;
		slab->in_empty_cache = 1;

		alloc_handle_t* handle = slab->handle;

		if(attr_unlikely(slab->alloc_limit == 1))
		{
			if(handle->slab)
			{
				handle->slab->prev = slab;
			}

			slab->next = handle->slab;
			slab->prev = NULL;
			handle->slab = slab;
		}

		if(++handle->empty_slabs > alloc_consts.slab.max_free_per_handle)
		{
			alloc_handle_ret_empty_slab(handle, slab);
		}

		return;
	}

	if(attr_unlikely(slab->count == slab->alloc_limit - 1))
	{
		alloc_handle_t* handle = slab->handle;

		if(handle->slab)
		{
			handle->slab->prev = slab;
			slab->next = handle->slab;
		}
		else
		{
			slab->next = NULL;
		}

		slab->prev = NULL;
		handle->slab = slab;
	}

	memcpy(ptr, &slab->free, sizeof(slab->free));
	slab->free = ptr;
}


void
alloc_free_foreign_internal(
	void* ptr,
	alloc_arena_header_t* arena,
	alloc_slab_header_t* slab
	)
{
	void* new_block = ptr;
	void* old_free = atomic_load_acq(&slab->foreign_free);

	do
	{
		if(attr_unlikely(old_free == (void*) -1))
		{
			if(atomic_fetch_sub_acq_rel(&arena->zombie_blocks, 1) == 1)
			{
				alloc_ret_arena(arena);
			}

			return;
		}

		memcpy(ptr, &old_free, sizeof(old_free));
	}
	while(!atomic_exchange_weak_acq_rel(&slab->foreign_free, &old_free, new_block));

	if(!old_free && atomic_load_acq(&arena->tid) != ALLOC_DEAD_THREAD_TID)
	{
		alloc_handle_t* handle = slab->handle;
		alloc_slab_header_t* old_head = atomic_load_acq(&handle->foreign_slab);

		do
		{
			slab->foreign_next = old_head;
		}
		while(!atomic_exchange_weak_acq_rel(&handle->foreign_slab, &old_head, slab));
	}
}


void
alloc_free_e(
	const volatile void* ptr,
	alloc_t size
	)
{
#ifndef ALLOC_RELEASE
	atomic_store_rel(&alloc_consts.first_use_done, 1);
#endif

	assert_ptr(ptr, size);

	if(attr_unlikely(!ptr))
	{
		return;
	}

	if(attr_unlikely(alloc_is_bootstrap_ptr(ptr)))
	{
		alloc_bootstrap_free(ptr, size);
		return;
	}

	alloc_log_debug("free(): ptr=", ptr, " size=", size, " tid=", alloc_tls.tid);

	alloc_ensure_tls();
	alloc_huge_maintenance(alloc_tls.numa, alloc_tls.numa_data_timer);

	if(attr_unlikely(alloc_is_size_virtual(size)))
	{
		ALLOC_VALGRIND_FREE(ptr);
		alloc_huge_free(ptr, size, alloc_tls.numa);
		return;
	}

	alloc_handle_idx_t handle_idx = alloc_get_handle_idx(size);
	const alloc_handle_t* handle = alloc_tls.handles + handle_idx;

	ALLOC_VALGRIND_DISABLE_ERROR_REPORTING();

	alloc_arena_header_t* arena = alloc_ptr_to_arena(ptr);

#ifndef ALLOC_RELEASE
	alloc_slab_idx_t slab_idx = (((void*) ptr - arena->data) & handle->slab_mask) >> alloc_consts.slab.small_shift;
	alloc_slab_header_t* slab_base = (void*) arena + alloc_consts.arena.slab_indexable_offset;
	alloc_slab_header_t* slab = slab_base + slab_idx;

	assert_eq(slab->idx, slab_idx + 1, alloc_free_trap(ptr, size, handle));
	assert_neq(slab->count, 0, alloc_free_trap(ptr, size, handle));
	assert_eq(slab->handle->alloc_size, handle->alloc_size, alloc_free_trap(ptr, size, handle));
	assert_eq(slab->handle->alignment, handle->alignment, alloc_free_trap(ptr, size, handle));
#endif

	alloc_red_zone_check(ptr - alloc_consts.red_zone.size, -1);
	alloc_red_zone_check(ptr + size, 1);

	bool is_own = atomic_load_acq(&arena->tid) == alloc_tls.tid;

	if(is_own && alloc_tcache_try_push_handle(handle_idx, handle, (void*) ptr))
	{
		ALLOC_VALGRIND_ENABLE_ERROR_REPORTING();
		ALLOC_VALGRIND_FREE(ptr);
		return;
	}

#ifdef ALLOC_RELEASE
	alloc_slab_idx_t slab_idx = (((void*) ptr - arena->data) & handle->slab_mask) >> alloc_consts.slab.small_shift;
	alloc_slab_header_t* slab_base = (void*) arena + alloc_consts.arena.slab_indexable_offset;
	alloc_slab_header_t* slab = slab_base + slab_idx;

	assert_eq(slab->idx, slab_idx + 1, alloc_free_trap(ptr, size, handle));
	assert_neq(slab->count, 0, alloc_free_trap(ptr, size, handle));
	assert_eq(slab->handle->alloc_size, handle->alloc_size, alloc_free_trap(ptr, size, handle));
	assert_eq(slab->handle->alignment, handle->alignment, alloc_free_trap(ptr, size, handle));
#endif

	if(is_own)
	{
		alloc_free_own_internal((void*) ptr, arena, slab);
	}
	else
	{
		alloc_free_foreign_internal((void*) ptr, arena, slab);
	}

	ALLOC_VALGRIND_ENABLE_ERROR_REPORTING();
	ALLOC_VALGRIND_FREE(ptr);
}


void
alloc_realloc_trap(
	const volatile void* ptr,
	alloc_t old_size,
	alloc_t new_size,
	const alloc_handle_t* old_handle
	)
{
	assert_not_null(ptr);
	assert_not_null(old_handle);

	alloc_log_error("realloc(): malformed arguments, ptr=", ptr,
		" old_size=", old_size, " new_size=", new_size, " old_handle->alloc_size=",
		old_handle->alloc_size, " old_handle->alignment=", old_handle->alignment);
}


void*
alloc_realloc_e(
	const volatile void* ptr,
	alloc_t old_size,
	alloc_t new_size,
	int zero
	)
{
	assert_ptr(ptr, old_size);

	alloc_log_debug("realloc(): ptr=", ptr, " old_size=", old_size,
		" new_size=", new_size, " zero=", zero, " tid=", alloc_tls.tid);

	if(attr_unlikely(!new_size))
	{
		alloc_free_e(ptr, old_size);
		return NULL;
	}

	if(attr_unlikely(!ptr))
	{
		return alloc_alloc_e(new_size, zero);
	}

	int old_virtual = alloc_is_size_virtual(old_size);
	int new_virtual = alloc_is_size_virtual(new_size);

#ifndef ALLOC_RELEASE
	if(!old_virtual && !alloc_is_bootstrap_ptr(ptr))
	{
		alloc_handle_idx_t old_handle_idx = alloc_get_handle_idx(old_size);
		const alloc_handle_t* old_handle = alloc_tls.handles + old_handle_idx;

		alloc_arena_header_t* arena = alloc_ptr_to_arena(ptr);
		alloc_slab_idx_t slab_idx = (((void*) ptr - arena->data) & old_handle->slab_mask) >> alloc_consts.slab.small_shift;
		alloc_slab_header_t* slab_base = (void*) arena + alloc_consts.arena.slab_indexable_offset;
		alloc_slab_header_t* slab = slab_base + slab_idx;

		assert_eq(slab->idx, slab_idx + 1, alloc_realloc_trap(ptr, old_size, new_size, old_handle));
		assert_neq(slab->count, 0, alloc_realloc_trap(ptr, old_size, new_size, old_handle));
		assert_eq(slab->handle->alloc_size, old_handle->alloc_size, alloc_realloc_trap(ptr, old_size, new_size, old_handle));
		assert_eq(slab->handle->alignment, old_handle->alignment, alloc_realloc_trap(ptr, old_size, new_size, old_handle));
	}
#endif

	if(attr_unlikely(old_virtual || new_virtual))
	{
		if(old_virtual && new_virtual)
		{
			void* new_ptr = alloc_realloc_virtual_e(ptr, old_size, new_size);
			if(attr_unlikely(!new_ptr))
			{
				return NULL;
			}

			alloc_report_virtual_free(old_size);
			alloc_report_virtual_alloc(new_size);

			if(new_size > old_size && zero)
			{
				alloc_t to = MACRO_MIN(new_size, MACRO_ALIGN_UP(old_size, alloc_consts.page.size));
				if(to != old_size)
				{
					memset((void*) new_ptr + old_size, 0, to - old_size);
				}
			}

			ALLOC_VALGRIND_FREE(ptr);
			ALLOC_VALGRIND_ALLOC(new_ptr, new_size, zero);
			ALLOC_VALGRIND_DEFINE(new_ptr, old_size);

			return new_ptr;
		}

		goto goto_fresh_realloc;
	}

	if(attr_unlikely(alloc_is_bootstrap_ptr(ptr)))
	{
		if(attr_unlikely(atomic_load_acq(&alloc_consts.state) < ALLOC_CONSTS_STATE_INITIALIZED))
		{
			return alloc_bootstrap_realloc(ptr, old_size, new_size, zero);
		}

		goto goto_fresh_realloc;
	}

	alloc_handle_idx_t old_handle_idx = alloc_get_handle_idx(old_size);
	alloc_handle_idx_t new_handle_idx = alloc_get_handle_idx(new_size);

	if(old_handle_idx == new_handle_idx)
	{
		ALLOC_VALGRIND_DISABLE_ERROR_REPORTING();

		alloc_red_zone_check(ptr - alloc_consts.red_zone.size, -1);
		alloc_red_zone_check(ptr + old_size, 1);

		if(new_size > old_size && zero)
		{
			memset((void*) ptr + old_size, 0, new_size - old_size);
		}

		alloc_red_zone_init((void*) ptr + new_size);

		ALLOC_VALGRIND_ENABLE_ERROR_REPORTING();
		ALLOC_VALGRIND_RESIZE(ptr, old_size, new_size);

		return (void*) ptr;
	}

goto_fresh_realloc:

	void* new_ptr = alloc_alloc_e(new_size, 0);
	if(attr_unlikely(!new_ptr))
	{
		return NULL;
	}

	memcpy(new_ptr, (void*) ptr, MACRO_MIN(old_size, new_size));

	if(new_size > old_size && zero)
	{
		memset((void*) new_ptr + old_size, 0, new_size - old_size);
	}

	alloc_free_e(ptr, old_size);

	return new_ptr;
}


void
alloc_thread_dtor_fn(
	void* arg
	)
{
	(void) arg;

	if(!alloc_tls.initialized)
	{
		return;
	}

	alloc_log_debug("thread_dtor(): tid=", alloc_tls.tid, " numa=", alloc_tls.numa);

	ALLOC_VALGRIND_DISABLE_ERROR_REPORTING();

	for(alloc_handle_idx_t class_idx = 0; class_idx < alloc_consts.handles.count; ++class_idx)
	{
		alloc_tcache_class_t* class = alloc_tls.tcache.classes + class_idx;
		for(alloc_t i = 0; i < class->count; ++i)
		{
			void* ptr = *class->head;
			class->head = alloc_tcache_slot(class, class->head - class->ptr + 1);

			alloc_arena_header_t* arena = alloc_ptr_to_arena(ptr);
			alloc_slab_idx_t slab_idx = (((void*) ptr - arena->data)
				& alloc_tls.handles[class_idx].slab_mask) >> alloc_consts.slab.small_shift;
			alloc_slab_header_t* slab_base = (void*) arena + alloc_consts.arena.slab_indexable_offset;
			alloc_slab_header_t* slab = slab_base + slab_idx;

			assert_eq(atomic_load_acq(&arena->tid), alloc_tls.tid);
			assert_eq(slab->idx, slab_idx + 1);
			assert_neq(slab->count, 0);

			alloc_free_own_internal(ptr, arena, slab);
			ALLOC_VALGRIND_FREE(ptr);
		}

		class->count = 0;
		class->head = class->ptr;
		class->tail = class->ptr;
	}

	for(alloc_arena_idx_t i = 0; i < alloc_tls.arenas.used; ++i)
	{
		alloc_arena_header_t* arena = alloc_tls.arenas.ptr[i];

		assert_eq(arena->local_idx, i, alloc_log_error("thread_dtor(): arena header corruption detected"));

		atomic_store_rel(&arena->zombie_blocks, (uint32_t) -1);
		atomic_store_rel(&arena->tid, ALLOC_DEAD_THREAD_TID);

		uint32_t total_active = 0;
		alloc_slab_header_t* slab_base = (void*) arena + alloc_consts.arena.slab_indexable_offset;

		for(alloc_slab_idx_t slab_idx = alloc_consts.arena.prefix_slabs; slab_idx < arena->used; ++slab_idx)
		{
			alloc_slab_header_t* slab = slab_base + slab_idx;
			if(slab->idx != slab_idx + 1 || !slab->count)
			{
				continue;
			}

			alloc_block_idx_t slab_active = slab->count;
			alloc_block_idx_t foreign_seen = 0;
			alloc_t slab_size = (alloc_t) 1 << slab->handle->slab_shift;
			void* slab_begin = slab->slab_data;
			void* slab_end = slab_begin + slab_size;

			void* block = atomic_exchange_acq(&slab->foreign_free, (void*) -1);
			while(block)
			{
				assert_neq(block, (void*) -1, alloc_log_error("thread_dtor(): malformed foreign list sentinel, tid=",
					alloc_tls.tid, " arena=", arena->local_idx, " slab_idx=", slab_idx, " slab_count=", slab->count,
					" slab_span=", slab->span, " slab_clean=", slab->clean, " slab_in_empty_cache=", slab->in_empty_cache,
					" slab_handle=", slab->handle, " slab_alloc_size=", slab->handle ? slab->handle->alloc_size : 0,
					" arena_used=", arena->used, " arena_count=", arena->count, " arena_clean=", arena->clean,
					" arena_in_empty=", arena->in_empty, " zombie_blocks=", atomic_load_acq(&arena->zombie_blocks)));

				assert_true(block >= slab_begin && block < slab_end,
					alloc_log_error("thread_dtor(): malformed foreign block ptr=", block,
					" tid=", alloc_tls.tid, " arena=", arena->local_idx, " slab_idx=", slab_idx,
					" slab_begin=", slab_begin, " slab_end=", slab_end));

				assert_true(++foreign_seen <= slab_active,
					alloc_log_error("thread_dtor(): foreign list longer than active count, tid=", alloc_tls.tid,
					" arena=", arena->local_idx, " slab_idx=", slab_idx, " active=", slab_active, " seen=", foreign_seen));

				void* next_block;
				memcpy(&next_block, block, sizeof(next_block));

				--slab->count;
				block = next_block;
			}

			total_active += slab->count;

			alloc_slab_idx_t span = MACRO_MAX(slab->span, 1);
			slab_idx += span - 1;
		}

		uint32_t bias_to_remove = (uint32_t) -1 - total_active;
		if(atomic_fetch_sub_acq_rel(&arena->zombie_blocks, bias_to_remove) == bias_to_remove)
		{
			alloc_ret_arena(arena);
		}
	}

	ALLOC_VALGRIND_ENABLE_ERROR_REPORTING();

	alloc_free_virtual(alloc_tls.arenas.ptr, alloc_tls.arenas.size);
	alloc_tcb_deref(alloc_tls.tcb);
	alloc_tls.handles = NULL;

	/* Other dtors that run after this one might request some memory. Reset here to be
	 * able to init again, which if it really happens will reschedule this dtor again.
	 */
	alloc_tls = (alloc_tls_t){0};
}


#ifndef ALLOC_RELEASE


	void
	alloc_check_red_zones_e(
		volatile const void* ptr,
		alloc_t size
		)
	{
		assert_ptr(ptr, size);

		if(attr_unlikely(
			!alloc_consts.red_zone.size ||
			alloc_is_bootstrap_ptr(ptr) ||
			alloc_is_size_virtual(size)
			))
		{
			return;
		}

		alloc_ensure_tls();

		const alloc_handle_t* handle = alloc_get_default_handle(size);
		assert_not_null(handle);

		ALLOC_VALGRIND_DISABLE_ERROR_REPORTING();

		alloc_red_zone_check(ptr - alloc_consts.red_zone.size, -1);
		alloc_red_zone_check(ptr + size, 1);

		ALLOC_VALGRIND_ENABLE_ERROR_REPORTING();
	}


#endif


alloc_handle_idx_t
alloc_test_default_idx_fn(
	alloc_t size
	)
{
	return alloc_default_idx_fn(size);
}


const alloc_handle_t*
alloc_test_get_handle(
	alloc_t size
	)
{
	if(attr_unlikely(size == 0))
	{
		return NULL;
	}

	if(alloc_is_size_virtual(size))
	{
		return NULL;
	}

	return alloc_get_default_handle(size);
}


alloc_t
alloc_test_find_size_for_class(
	alloc_t target_class_idx
	)
{
	for(alloc_t size = 1; size <= alloc_consts.slab.max_nonvirtual_size; ++size)
	{
		const alloc_handle_t* handle = alloc_test_get_handle(size);
		if(handle && handle->slab_class == target_class_idx)
		{
			return size;
		}
	}

	alloc_log_error("test_find_size_for_class(): class idx not found, target_class_idx=", target_class_idx);
	hard_assert_unreachable();
}


alloc_arena_header_t*
alloc_test_get_arena_from_ptr(
	const volatile void* ptr
	)
{
	assert_not_null(ptr);

	return alloc_ptr_to_arena(ptr);
}

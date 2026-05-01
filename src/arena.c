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
#include <alloc/sync.h>
#include <alloc/btree.h>
#include <alloc/debug.h>
#include <alloc/macro.h>
#include <alloc/types.h>
#include <alloc/atomic.h>
#include <alloc/consts.h>
#include <alloc/platform.h>

#include <stdint.h>
#include <string.h>


void
alloc_init_arena(
	alloc_arena_header_t* arena,
	alloc_tcb_t* tcb,
	void* data,
	alloc_arena_idx_t pool_idx,
	uint16_t numa,
	int clean
	)
{
	assert_not_null(arena);
	assert_not_null(tcb);

	uint64_t* btree = (void*) arena + alloc_consts.arena.btree_offset;
	alloc_arena_header_t** vacant_next = (void*) arena + alloc_consts.arena.vacant_next_offset;
	alloc_arena_header_t** vacant_prev = (void*) arena + alloc_consts.arena.vacant_prev_offset;
	alloc_slab_header_t* slab_headers = (void*) arena + alloc_consts.arena.slab_indexable_offset;

	arena->tcb = tcb;
	arena->data = data;
	arena->pool_idx = pool_idx;
	arena->tid = tcb->tid;
	arena->numa = numa;
	arena->used = 0;
	arena->count = 0;
	arena->in_empty = 0;
	arena->clean = clean;
	arena->free = 0;
	arena->free_base_blocks = alloc_consts.arena.slab_count;
	arena->btree_avail_mask = alloc_consts.arena.btree_avail_mask_template;
	arena->vacant_mask = 0;
	atomic_store_rx(&arena->zombie_blocks, 0);

	memcpy(btree, alloc_consts.arena.btree_template, alloc_btree_size());
	memset(vacant_next, 0, sizeof(*vacant_next) * alloc_consts.slab.class_count);
	memset(vacant_prev, 0, sizeof(*vacant_prev) * alloc_consts.slab.class_count);

	if(!clean)
	{
		memset(slab_headers, 0, sizeof(*slab_headers) * alloc_consts.arena.slab_count);
	}
}


void
alloc_init_arena_pool(
	alloc_numa_local_data_t* local_data,
	uint16_t numa
	)
{
	assert_not_null(local_data);

	alloc_t data_region_size = alloc_consts.arena.data_region_size;
	alloc_t prefix_region_size = alloc_consts.arena.virtual_capacity * alloc_consts.arena.prefix_size;
	alloc_t pool_region_size = data_region_size * 2 + prefix_region_size;
	int thp_enable = alloc_consts.arena.thp_enable || alloc_consts.arena.prefix_thp_enable;

	void* pool_region = alloc_alloc_virtual_e(pool_region_size, 0, numa, thp_enable);
	hard_assert_not_null(pool_region);

	void* pool_first_slab = MACRO_ALIGN_UP(pool_region, data_region_size - 1);

	local_data->numa = numa;
	local_data->arena.pool_region_size = pool_region_size;
	local_data->arena.pool_region = pool_region;
	local_data->arena.pool_first_slab = pool_first_slab;
	local_data->arena.slot_count = alloc_consts.arena.virtual_capacity;
	local_data->arena.used_slots = 0;
	local_data->arena.committed_slots = 0;
	local_data->arena.prefix_committed_slots = 0;
	local_data->arena.high_watermark = 0;
	local_data->arena.prefix_high_watermark = 0;
	local_data->arena.link_tail = NULL;

	memset(local_data->arena.bitmap_l0, 0xFF, sizeof(uint64_t) * alloc_consts.numa.arena_bitmap_l0_size);
	local_data->arena.bitmap_l0[alloc_consts.numa.arena_bitmap_l0_size] = 0xDEADBEEFDEADBEEF;

	alloc_t rem_l0 = local_data->arena.slot_count & 63;
	if(rem_l0)
	{
		local_data->arena.bitmap_l0[alloc_consts.numa.arena_bitmap_l0_size - 1] &= ((uint64_t) 1 << rem_l0) - 1;
	}

	memset(local_data->arena.bitmap_l1, 0xFF, sizeof(uint64_t) * alloc_consts.numa.arena_bitmap_l1_size);
	local_data->arena.bitmap_l1[alloc_consts.numa.arena_bitmap_l1_size] = 0xDEADBEEFDEADBEEF;

	alloc_t rem_l1 = alloc_consts.numa.arena_bitmap_l0_size & 63;
	if(rem_l1)
	{
		local_data->arena.bitmap_l1[alloc_consts.numa.arena_bitmap_l1_size - 1] &= ((uint64_t) 1 << rem_l1) - 1;
	}

	alloc_t preallocate = alloc_consts.arena.preallocate;
	if(preallocate)
	{
		if(!alloc_commit_virtual_e(local_data->arena.pool_first_slab, alloc_consts.arena.size * preallocate))
		{
			return;
		}

		local_data->arena.committed_slots = preallocate;

		void* prefix_raw_base = local_data->arena.pool_first_slab + alloc_consts.arena.prefix_region_offset;
		void* prefix_raw_end = prefix_raw_base + preallocate * alloc_consts.arena.prefix_size;
		void* prefix_commit_end = MACRO_ALIGN_UP(prefix_raw_end, alloc_consts.page.mask);

		alloc_t prefix_commit_size = prefix_commit_end - prefix_raw_base;
		if(prefix_commit_size && !alloc_commit_virtual_e(prefix_raw_base, prefix_commit_size))
		{
			return;
		}

		local_data->arena.prefix_committed_slots = preallocate;
	}
}


alloc_arena_header_t*
alloc_slot_to_arena_prefix(
	const alloc_numa_local_data_t* local_data,
	alloc_t slot
	)
{
	void* prefix_base = local_data->arena.pool_first_slab + alloc_consts.arena.prefix_region_offset;
	return prefix_base + slot * alloc_consts.arena.prefix_size;
}


void*
alloc_slot_to_arena_data(
	const alloc_numa_local_data_t* local_data,
	alloc_t slot
	)
{
	return local_data->arena.pool_first_slab + (slot << alloc_consts.arena.shift);
}


bool
alloc_take_slot(
	alloc_numa_local_data_t* local_data,
	alloc_t* out_slot
	)
{
	assert_not_null(local_data);
	assert_not_null(out_slot);

	uint64_t* cur_l1 = local_data->arena.bitmap_l1;
	while(!*cur_l1) ++cur_l1;

	if(attr_unlikely(cur_l1 == local_data->arena.bitmap_l1 + alloc_consts.numa.arena_bitmap_l1_size))
	{
		return false;
	}

	uint64_t l1_word = *cur_l1;
	alloc_t l1_bit = MACRO_CTZ(l1_word);
	alloc_t l0_idx = ((cur_l1 - local_data->arena.bitmap_l1) << 6) + l1_bit;
	assert_lt(l0_idx, alloc_consts.numa.arena_bitmap_l0_size);

	uint64_t l0_word = local_data->arena.bitmap_l0[l0_idx];
	assert_true(l0_word);

	alloc_t l0_bit = MACRO_CTZ(l0_word);
	uint64_t new_l0 = l0_word & ~((uint64_t) 1 << l0_bit);
	local_data->arena.bitmap_l0[l0_idx] = new_l0;
	if(!new_l0)
	{
		*cur_l1 &= ~((uint64_t) 1 << l1_bit);
	}

	alloc_t slot = (l0_idx << 6) + l0_bit;
	assert_lt(slot, local_data->arena.slot_count);

	*out_slot = slot;
	++local_data->arena.used_slots;
	return true;
}


bool
alloc_ensure_slot_committed(
	alloc_numa_local_data_t* local_data,
	alloc_t slot
	)
{
	assert_not_null(local_data);

	if(slot < local_data->arena.committed_slots && slot < local_data->arena.prefix_committed_slots)
	{
		return true;
	}

	alloc_t data_commit_from = local_data->arena.committed_slots;
	alloc_t data_commit_to = MACRO_MIN(data_commit_from + alloc_consts.arena.commit_batch, local_data->arena.slot_count);
	if(slot >= data_commit_from)
	{
		void* data_commit_base = local_data->arena.pool_first_slab + data_commit_from * alloc_consts.arena.size;
		alloc_t data_commit_size = (data_commit_to - data_commit_from) << alloc_consts.arena.shift;
		if(!alloc_commit_virtual_e(data_commit_base, data_commit_size))
		{
			return false;
		}

		local_data->arena.committed_slots = data_commit_to;
	}

	alloc_t prefix_commit_from = local_data->arena.prefix_committed_slots;
	alloc_t prefix_commit_to = MACRO_MIN(prefix_commit_from + alloc_consts.arena.commit_batch, local_data->arena.slot_count);
	if(slot >= prefix_commit_from)
	{
		void* prefix_base = local_data->arena.pool_first_slab + alloc_consts.arena.prefix_region_offset;
		void* prefix_commit_raw_base = prefix_base + prefix_commit_from * alloc_consts.arena.prefix_size;
		void* prefix_commit_raw_end = prefix_base + prefix_commit_to * alloc_consts.arena.prefix_size;
		void* prefix_commit_base = MACRO_ALIGN_DOWN(prefix_commit_raw_base, alloc_consts.page.mask);
		void* prefix_commit_end = MACRO_ALIGN_UP(prefix_commit_raw_end, alloc_consts.page.mask);

		alloc_t prefix_commit_size = prefix_commit_end - prefix_commit_base;
		if(prefix_commit_size && !alloc_commit_virtual_e(prefix_commit_base, prefix_commit_size))
		{
			return false;
		}

		local_data->arena.prefix_committed_slots = prefix_commit_to;
	}

	alloc_log_info("arena_commit_batch(): numa=", local_data->numa,
		" slot=", slot,
		" committed_from=", data_commit_from,
		" committed_to=", local_data->arena.committed_slots,
		" commit_slots=", local_data->arena.committed_slots - data_commit_from,
		" used_slots=", local_data->arena.used_slots,
		" high_watermark=", local_data->arena.high_watermark);

	return true;
}


void
alloc_undo_take_slot(
	alloc_numa_local_data_t* local_data,
	alloc_t slot
	)
{
	alloc_t l0_idx = slot >> 6;
	alloc_t l0_bit = slot & 63;
	local_data->arena.bitmap_l0[l0_idx] |= (uint64_t) 1 << l0_bit;
	local_data->arena.bitmap_l1[l0_idx >> 6] |= (uint64_t) 1 << (l0_idx & 63);
	assert_neq(local_data->arena.used_slots--, 0);
}


void
alloc_link_insert_new_slot(
	alloc_numa_local_data_t* local_data,
	alloc_arena_header_t* arena
	)
{
	arena->link_prev = local_data->arena.link_tail;
	arena->link_next = NULL;

	if(local_data->arena.link_tail)
	{
		local_data->arena.link_tail->link_next = arena;
	}

	local_data->arena.link_tail = arena;
}


void
alloc_link_split_free_region(
	alloc_numa_local_data_t* local_data,
	alloc_arena_header_t* arena,
	alloc_t slot
	)
{
	alloc_arena_header_t* old_next = arena->link_next;
	alloc_t region_end;

	if(old_next)
	{
		region_end = old_next->pool_idx;
	}
	else
	{
		region_end = local_data->arena.high_watermark;
	}

	if(slot + 1 >= region_end)
	{
		return;
	}

	alloc_arena_header_t* rest = alloc_slot_to_arena_prefix(local_data, slot + 1);
	rest->link_prev = arena;
	rest->link_next = old_next;

	if(old_next)
	{
		old_next->link_prev = rest;
	}
	else
	{
		local_data->arena.link_tail = rest;
	}

	arena->link_next = rest;
}


void
alloc_link_slot(
	alloc_numa_local_data_t* local_data,
	uint32_t slot
	)
{
	alloc_arena_header_t* arena = alloc_slot_to_arena_prefix(local_data, slot);

	if(slot >= local_data->arena.high_watermark)
	{
		alloc_link_insert_new_slot(local_data, arena);
		local_data->arena.high_watermark = slot + 1;
		local_data->arena.prefix_high_watermark = slot + 1;
	}
	else
	{
		alloc_link_split_free_region(local_data, arena, slot);
	}
}


alloc_arena_header_t*
alloc_get_arena(
	alloc_tcb_t* tcb,
	uint16_t numa
	)
{
	alloc_numa_local_data_t* local_data = alloc_get_numa_local_data(numa);

	sync_mtx_lock(&local_data->arena.mtx);

	alloc_t old_committed = local_data->arena.committed_slots;

	alloc_t slot;
	int success = alloc_take_slot(local_data, &slot);
	if(success)
	{
		if(alloc_ensure_slot_committed(local_data, slot))
		{
			alloc_link_slot(local_data, slot);
		}
		else
		{
			alloc_undo_take_slot(local_data, slot);
			success = false;
		}
	}

	sync_mtx_unlock(&local_data->arena.mtx);

	if(!success)
	{
		if(local_data->arena.used_slots < local_data->arena.slot_count)
		{
			alloc_log_error("get_arena(): failed to commit arena slot for numa=", numa,
				" slot_count=", local_data->arena.slot_count,
				" used_slots=", local_data->arena.used_slots,
				" committed_slots=", local_data->arena.committed_slots,
				" commit_batch=", alloc_consts.arena.commit_batch);
			return NULL;
		}

		uint64_t exhausted = atomic_fetch_add_acq_rel(&local_data->arena.exhausted_events, 1) + 1;
		if(exhausted == 1 || !(exhausted & (exhausted - 1)) || !(exhausted % 100000))
		{
			alloc_log_error("get_arena(): arena pool capacity exhausted for numa=", numa,
				" exhaustion_count=", exhausted,
				" slot_count=", local_data->arena.slot_count,
				" used_slots=", local_data->arena.used_slots,
				" committed_slots=", local_data->arena.committed_slots,
				" high_watermark=", local_data->arena.high_watermark);
		}

		return NULL;
	}

	alloc_arena_header_t* arena = alloc_slot_to_arena_prefix(local_data, slot);
	void* data = alloc_slot_to_arena_data(local_data, slot);
	alloc_init_arena(arena, tcb, data, slot, numa, slot >= old_committed);

	alloc_tcb_ref(tcb);

	return arena;
}


bool
alloc_slot_is_free(
	const alloc_numa_local_data_t* local_data,
	alloc_t slot
	)
{
	alloc_t l0_idx = slot >> 6;
	alloc_t l0_bit = slot & 63;
	return !!(local_data->arena.bitmap_l0[l0_idx] & ((uint64_t) 1 << l0_bit));
}


void
alloc_ret_slot(
	alloc_numa_local_data_t* local_data,
	alloc_t slot
	)
{
	assert_not_null(local_data);
	assert_lt(slot, local_data->arena.slot_count);

	alloc_t l0_idx = slot >> 6;
	alloc_t l0_bit = slot & 63;
	uint64_t bit = (uint64_t) 1 << l0_bit;
	uint64_t l0_word = local_data->arena.bitmap_l0[l0_idx];
	assert_false(l0_word & bit);

	local_data->arena.bitmap_l0[l0_idx] = l0_word | bit;
	local_data->arena.bitmap_l1[l0_idx >> 6] |= (uint64_t) 1 << (l0_idx & 63);
	assert_neq(local_data->arena.used_slots--, 0);

	alloc_arena_header_t* arena = alloc_slot_to_arena_prefix(local_data, slot);
	alloc_arena_header_t* prev = arena->link_prev;
	alloc_arena_header_t* next = arena->link_next;

	if(next && next->free)
	{
		arena->link_next = next->link_next;

		if(next->link_next)
		{
			next->link_next->link_prev = arena;
		}
		else
		{
			local_data->arena.link_tail = arena;
		}
	}

	if(prev && prev->free)
	{
		prev->link_next = arena->link_next;

		if(arena->link_next)
		{
			arena->link_next->link_prev = prev;
		}
		else
		{
			local_data->arena.link_tail = prev;
		}
	}

	arena->free = 1;
}


void
alloc_try_tail_decommit(
	alloc_numa_local_data_t* local_data
	)
{
	if(!alloc_consts.arena.tail_decommit_enable && !alloc_consts.arena.prefix_tail_decommit_enable)
	{
		return;
	}

	uint32_t hwm = local_data->arena.high_watermark;
	if(!hwm)
	{
		return;
	}

	alloc_arena_header_t* tail = local_data->arena.link_tail;
	if(!tail || !tail->free)
	{
		return;
	}

	uint32_t tail_start = tail->pool_idx;
	assert_lt(tail_start, hwm);

	uint32_t used = local_data->arena.used_slots;
	uint32_t tail_free = hwm - tail_start;
	uint32_t shrink = 0;
	if(alloc_consts.arena.tail_decommit_enable && used * alloc_consts.arena.tail_decommit_trigger_div < hwm)
	{
		uint32_t keep_for_ratio = used * (alloc_consts.arena.tail_decommit_amount_div + 1);
		uint32_t min_keep = alloc_consts.arena.preallocate;
		if(min_keep < tail_start)
		{
			min_keep = tail_start;
		}

		uint32_t target_hwm = keep_for_ratio > min_keep ? keep_for_ratio : min_keep;
		if(target_hwm < hwm)
		{
			shrink = hwm - target_hwm;
			uint32_t unit_slots = alloc_consts.arena.align >> alloc_consts.arena.shift;
			shrink = (shrink / unit_slots) * unit_slots;
			if(shrink > tail_free)
			{
				shrink = tail_free;
			}
		}
	}

	uint32_t prefix_shrink = 0;
	if(alloc_consts.arena.prefix_tail_decommit_enable && used * alloc_consts.arena.prefix_tail_decommit_trigger_div < hwm)
	{
		uint32_t keep_for_ratio = used * (alloc_consts.arena.prefix_tail_decommit_amount_div + 1);
		uint32_t min_keep = alloc_consts.arena.preallocate;
		if(min_keep < tail_start)
		{
			min_keep = tail_start;
		}

		uint32_t target_hwm = keep_for_ratio > min_keep ? keep_for_ratio : min_keep;
		if(target_hwm < hwm)
		{
			prefix_shrink = hwm - target_hwm;
			if(prefix_shrink > tail_free)
			{
				prefix_shrink = tail_free;
			}
		}
	}

	if(!shrink && !prefix_shrink)
	{
		return;
	}

	uint32_t new_hwm = hwm - shrink;
	uint32_t prefix_new_hwm = hwm - prefix_shrink;
	alloc_arena_header_t* tail_prev = tail->link_prev;

	if(shrink)
	{
		void* tail_base = local_data->arena.pool_first_slab + new_hwm * alloc_consts.arena.size;
		alloc_decommit_virtual_e(tail_base, shrink * alloc_consts.arena.size);
	}

	if(prefix_shrink)
	{
		void* prefix_base = local_data->arena.pool_first_slab + alloc_consts.arena.prefix_region_offset;
		void* prefix_raw_base = prefix_base + prefix_new_hwm * alloc_consts.arena.prefix_size;
		void* prefix_raw_end = prefix_base + hwm * alloc_consts.arena.prefix_size;
		void* prefix_tail_base = MACRO_ALIGN_UP(prefix_raw_base, alloc_consts.page.mask);
		void* prefix_tail_end = MACRO_ALIGN_DOWN(prefix_raw_end, alloc_consts.page.mask);

		if(prefix_tail_end > prefix_tail_base)
		{
			alloc_decommit_virtual_e(prefix_tail_base, prefix_tail_end - prefix_tail_base);
		}
	}

	if(shrink)
	{
		local_data->arena.high_watermark = new_hwm;

		if(local_data->arena.committed_slots > new_hwm)
		{
			local_data->arena.committed_slots = new_hwm;
		}
	}

	if(prefix_shrink)
	{
		local_data->arena.prefix_high_watermark = prefix_new_hwm;

		if(local_data->arena.prefix_committed_slots > prefix_new_hwm)
		{
			local_data->arena.prefix_committed_slots = prefix_new_hwm;
		}
	}

	if(shrink && shrink == tail_free)
	{
		if(tail_prev)
		{
			tail_prev->link_next = NULL;
		}

		local_data->arena.link_tail = tail_prev;
	}

	alloc_log_info("arena_tail_decommit(): numa=", local_data->numa,
		" used_slots=", used,
		" tail_start=", tail_start,
		" high_watermark_old=", hwm,
		" high_watermark_new=", new_hwm,
		" decommitted_slots=", shrink,
		" committed_slots=", local_data->arena.committed_slots);
}


void
alloc_ret_arena(
	alloc_arena_header_t* arena
	)
{
	alloc_tcb_deref(arena->tcb);

	alloc_numa_local_data_t* local_data = alloc_get_numa_local_data(arena->numa);

	sync_mtx_lock(&local_data->arena.mtx);
		alloc_ret_slot(local_data, arena->pool_idx);
		alloc_try_tail_decommit(local_data);
	sync_mtx_unlock(&local_data->arena.mtx);
}

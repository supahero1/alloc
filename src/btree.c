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

#include <alloc/btree.h>
#include <alloc/debug.h>
#include <alloc/macro.h>
#include <alloc/types.h>
#include <alloc/consts.h>


alloc_t
alloc_btree_size(
	void
	)
{
	return sizeof(uint64_t) * alloc_consts.slab.class_count;
}


void
alloc_btree_init(
	uint64_t* avail
	)
{
	for(alloc_t c = 0; c < alloc_consts.slab.class_count; ++c)
	{
		assert_eq(avail[c], 0);
	}

	alloc_t current_base_idx = 0;
	alloc_t remaining_base_blocks = alloc_consts.arena.slab_count;

	while(remaining_base_blocks)
	{
		alloc_t best_c = alloc_consts.slab.class_count;
		alloc_t best_blocks = 0;

		const alloc_t* shift_ptr = alloc_consts.slab.class_span_shift + alloc_consts.slab.class_count - 1;

		for(alloc_t class = alloc_consts.slab.class_count; class; --class)
		{
			alloc_t idx = class - 1;
			alloc_t blocks = (alloc_t) 1 << *(shift_ptr--);
			assert_neq(blocks, 0);

			if(blocks > remaining_base_blocks || (current_base_idx & (blocks - 1)))
			{
				continue;
			}

			best_c = idx;
			best_blocks = blocks;
			break;
		}

		hard_assert_lt(best_c, alloc_consts.slab.class_count);
		hard_assert_true(best_blocks);

		alloc_t idx_at_c = current_base_idx >> alloc_consts.slab.class_span_shift[best_c];
		avail[best_c] |= (uint64_t) 1 << idx_at_c;

		current_base_idx += best_blocks;
		remaining_base_blocks -= best_blocks;
	}
}


alloc_t
alloc_btree_get(
	uint64_t* avail,
	alloc_t slab_idx,
	uint32_t* avail_mask,
	uint32_t* changed_mask
	)
{
	uint32_t changed = 0;

	assert_neq(*avail_mask & (~(uint32_t) 0 << slab_idx), 0);
	alloc_t found_lvl = MACRO_CTZ(*avail_mask & (~(uint32_t) 0 << slab_idx));
	alloc_t found_idx = MACRO_CTZ(avail[found_lvl]);

	avail[found_lvl] &= ~((uint64_t) 1 << found_idx);

	if(!avail[found_lvl])
	{
		changed |= (uint32_t) 1 << found_lvl;
		*avail_mask &= ~((uint32_t) 1 << found_lvl);
	}

	if(found_lvl > slab_idx)
	{
		const alloc_t* shift_ptr = alloc_consts.slab.class_span_shift + found_lvl;

		for(alloc_t parent = found_lvl; parent > slab_idx; --parent)
		{
			alloc_t parent_shift = *(shift_ptr--);
			alloc_t child_shift = *shift_ptr;
			alloc_t child = parent - 1;
			assert_gt(parent_shift, child_shift);

			alloc_t ratio_shift = parent_shift - child_shift;
			alloc_t ratio = (alloc_t) 1 << ratio_shift;
			assert_le(ratio, 64);

			uint64_t sibling_mask = (((uint64_t) 1 << ratio) - 1) & ~((uint64_t) 1);

			uint64_t old_avail = avail[child];
			alloc_t child_start_idx = found_idx << ratio_shift;
			avail[child] |= sibling_mask << child_start_idx;

			if(!old_avail && avail[child])
			{
				uint32_t bit = (uint32_t) 1 << child;
				if(!(*avail_mask & bit))
				{
					changed |= bit;
				}
				*avail_mask |= bit;
			}

			found_idx = child_start_idx;
		}
	}

	if(changed_mask)
	{
		*changed_mask = changed;
	}

	return found_idx << alloc_consts.slab.class_span_shift[slab_idx];
}


void
alloc_btree_ret(
	uint64_t* avail,
	alloc_t slab_idx,
	alloc_t block_idx,
	uint32_t* avail_mask,
	uint32_t* changed_mask
	)
{
	uint32_t changed = 0;

	const alloc_t* shift_ptr = alloc_consts.slab.class_span_shift + slab_idx;
	alloc_t current_idx = block_idx >> alloc_consts.slab.class_span_shift[slab_idx];

	for(alloc_t parent = slab_idx; parent < alloc_consts.slab.class_count - 1; ++parent)
	{
		alloc_t child_shift = *(shift_ptr++);
		alloc_t parent_shift = *shift_ptr;
		uint32_t bit = (uint32_t) 1 << parent;
		int had_any = avail[parent] != 0;
		assert_gt(parent_shift, child_shift);

		alloc_t ratio_shift = parent_shift - child_shift;
		alloc_t ratio = (alloc_t) 1 << ratio_shift;
		assert_le(ratio, 64);

		uint64_t full_mask = ((uint64_t) 1 << ratio) - 1;
		uint64_t align_mask = ~(ratio - 1);
		alloc_t sibling_start = current_idx & align_mask;
		uint64_t mask = full_mask << sibling_start;

		uint64_t current_avail = avail[parent] | ((uint64_t) 1 << current_idx);

		if((current_avail & mask) != mask)
		{
			avail[parent] = current_avail;

			if(!had_any)
			{
				changed |= bit;
			}

			*avail_mask |= (uint32_t) 1 << parent;

			if(changed_mask)
			{
				*changed_mask = changed;
			}

			return;
		}

		current_avail &= ~mask;
		avail[parent] = current_avail;

		if(!current_avail)
		{
			if(had_any)
			{
				changed |= bit;
			}

			*avail_mask &= ~((uint32_t) 1 << parent);
		}

		current_idx >>= ratio_shift;
	}

	alloc_t top = alloc_consts.slab.class_count - 1;
	uint32_t top_bit = (uint32_t) 1 << top;
	int top_was_empty = avail[top] == 0;

	avail[top] |= (uint64_t) 1 << current_idx;

	if(top_was_empty)
	{
		changed |= top_bit;
	}

	*avail_mask |= top_bit;

	if(changed_mask)
	{
		*changed_mask = changed;
	}
}

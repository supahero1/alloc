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

#include <alloc/sync.h>
#include <alloc/debug.h>
#include <alloc/macro.h>
#include <alloc/types.h>
#include <alloc/atomic.h>
#include <alloc/consts.h>
#include <alloc/platform.h>
#include <alloc/valgrind.h>

#include <stdint.h>
#include <string.h>


void
alloc_tcb_init(
	alloc_tcb_t* tcb
	)
{
	assert_not_null(tcb);
	assert_le(alloc_consts.handles.count, ALLOC_MAX_HANDLE_COUNT);

	memcpy(tcb->handles, alloc_consts.handles.templates, sizeof(*tcb->handles) * alloc_consts.handles.count);
}


alloc_tcb_t*
alloc_tcb_acquire(
	uint16_t numa
	)
{
	alloc_numa_local_data_t* local = alloc_get_numa_local_data(numa);

	ALLOC_VALGRIND_DISABLE_ERROR_REPORTING();

	sync_mtx_lock(&local->tcb.mtx);

	alloc_tcb_t* tcb;
	alloc_t idx;

	if(local->tcb.free)
	{
		tcb = local->tcb.free;
		idx = alloc_tid_idx(tcb->tid);
		local->tcb.free = tcb->next_free;
	}
	else
	{
		idx = local->tcb.count++;
		hard_assert_lt(idx, alloc_consts.tcb.capacity);

		if(idx >= local->tcb.committed)
		{
			alloc_tcb_t* commit_ptr = local->tcb.ptr + local->tcb.committed;
			alloc_t space = alloc_consts.tcb.capacity - local->tcb.committed;
			alloc_t commit_size = MACRO_MIN(alloc_consts.tcb.commit_batch, space);
			if(commit_size)
			{
				void* commit_begin = MACRO_ALIGN_DOWN(commit_ptr, alloc_consts.page.mask);
				void* commit_end = commit_ptr + commit_size;
				local->tcb.committed += commit_size;

				int status = alloc_commit_virtual_e(commit_begin, commit_end - commit_begin);
				hard_assert_true(status);
			}
		}

		tcb = local->tcb.ptr + idx;
	}

	sync_mtx_unlock(&local->tcb.mtx);

	ALLOC_VALGRIND_ENABLE_ERROR_REPORTING();
	ALLOC_VALGRIND_INTERNAL_ALLOC(tcb, sizeof(*tcb));

	atomic_store_rx(&tcb->ref_count, 1);
	tcb->tid = alloc_make_tid(idx, numa);

	return tcb;
}


void
alloc_tcb_ref(
	alloc_tcb_t* tcb
	)
{
	atomic_fetch_add_acq_rel(&tcb->ref_count, 1);
}


void
alloc_tcb_deref(
	alloc_tcb_t* tcb
	)
{
	if(atomic_fetch_sub_acq_rel(&tcb->ref_count, 1) != 1)
	{
		return;
	}

	uint16_t numa = alloc_tid_numa(tcb->tid);
	alloc_numa_local_data_t* local = alloc_get_numa_local_data(numa);

	sync_mtx_lock(&local->tcb.mtx);
	tcb->next_free = local->tcb.free;
	local->tcb.free = tcb;
	sync_mtx_unlock(&local->tcb.mtx);

	ALLOC_VALGRIND_INTERNAL_FREE(tcb);
}

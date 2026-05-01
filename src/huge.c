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

#include <alloc/huge.h>
#include <alloc/sync.h>
#include <alloc/types.h>
#include <alloc/atomic.h>
#include <alloc/consts.h>
#include <alloc/report.h>
#include <alloc/platform.h>

#include <string.h>
#include <pthread.h>

#define ALLOC_HUGE_DEFAULT_TICKS_PER_NS_Q32 ((uint64_t) 2 << 32)


typedef struct alloc_huge
{
	sync_mtx_t mtx;
	uint8_t _Atomic initialized;
	uint8_t _Atomic calibrated;
	uint64_t _Atomic ticks_per_ns_q32;
	uint64_t start_tsc;
	uint64_t start_ns;
	uint64_t calibrate_deadline_tsc;
}
alloc_huge_t;


alloc_huge_t alloc_huge =
{
	.mtx = PTHREAD_MUTEX_INITIALIZER,
	.ticks_per_ns_q32 = ALLOC_HUGE_DEFAULT_TICKS_PER_NS_Q32
};


uint64_t
alloc_huge_read_tsc(
	void
	)
{
#if defined(__x86_64__) || defined(__i386__)
	uint32_t lo;
	uint32_t hi;
	__asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
	return ((uint64_t) hi << 32) | lo;
#elif defined(__aarch64__)
	uint64_t tsc;
	__asm__ volatile("isb\n\tmrs %0, cntvct_el0" : "=r"(tsc));
	return tsc;
#elif defined(__arm__)
	uint32_t lo;
	uint32_t hi;
	__asm__ volatile("isb\n\tmrrc p15, 1, %0, %1, c14" : "=r"(lo), "=r"(hi));
	return ((uint64_t) hi << 32) | lo;
#else
	#error Unsupported architecture.
#endif
}


uint64_t
alloc_huge_add_sat_u64(
	uint64_t base,
	uint64_t add
	)
{
	return add > UINT64_MAX - base ? UINT64_MAX : base + add;
}


uint64_t
alloc_huge_ns_to_ticks_q32(
	uint64_t ns,
	uint64_t ticks_per_ns_q32
	)
{
	return ((unsigned __int128) ns * ticks_per_ns_q32) >> 32;
}


uint64_t
alloc_huge_ns_to_ticks(
	uint64_t ns
	)
{
	return alloc_huge_ns_to_ticks_q32(ns, atomic_load_rx(&alloc_huge.ticks_per_ns_q32));
}


void
alloc_huge_calibrate_locked(
	)
{
	uint64_t now_tsc = alloc_huge_read_tsc();
	uint64_t now_ns = alloc_read_time_ns();
	uint64_t delta_tsc = now_tsc - alloc_huge.start_tsc;
	uint64_t delta_ns = now_ns - alloc_huge.start_ns;
	uint64_t ticks_per_ns_q32 = atomic_load_rx(&alloc_huge.ticks_per_ns_q32);

	if(delta_tsc && delta_ns)
	{
		ticks_per_ns_q32 = ((unsigned __int128) delta_tsc << 32) / delta_ns;
		if(attr_unlikely(!ticks_per_ns_q32))
		{
			ticks_per_ns_q32 = 1;
		}
	}

	atomic_store_rx(&alloc_huge.ticks_per_ns_q32, ticks_per_ns_q32);
	atomic_store_rel(&alloc_huge.calibrated, 1);
}


void
alloc_huge_try_calibrate(
	void
	)
{
	if(attr_likely(atomic_load_acq(&alloc_huge.calibrated)))
	{
		return;
	}

	sync_mtx_lock(&alloc_huge.mtx);

	if(attr_unlikely(!atomic_load_acq(&alloc_huge.calibrated)))
	{
		alloc_huge_calibrate_locked();
	}

	sync_mtx_unlock(&alloc_huge.mtx);
}


void
alloc_huge_global_init(
	void
	)
{
	if(attr_likely(atomic_load_acq(&alloc_huge.initialized)))
	{
		return;
	}

	sync_mtx_lock(&alloc_huge.mtx);

	if(attr_unlikely(!atomic_load_acq(&alloc_huge.initialized)))
	{
		uint64_t start_tsc = alloc_huge_read_tsc();
		uint64_t start_ns = alloc_read_time_ns();
		uint64_t calibrate_delta = alloc_huge_ns_to_ticks_q32(
			alloc_consts.huge.calibrate_ns, ALLOC_HUGE_DEFAULT_TICKS_PER_NS_Q32);

		alloc_huge.start_tsc = start_tsc;
		alloc_huge.start_ns = start_ns;
		alloc_huge.calibrate_deadline_tsc = alloc_huge_add_sat_u64(start_tsc, calibrate_delta);
		atomic_store_rx(&alloc_huge.ticks_per_ns_q32, ALLOC_HUGE_DEFAULT_TICKS_PER_NS_Q32);
		atomic_store_rx(&alloc_huge.calibrated, 0);
		atomic_store_rel(&alloc_huge.initialized, 1);
	}

	sync_mtx_unlock(&alloc_huge.mtx);
}


uint64_t
alloc_huge_threshold_tsc(
	void
	)
{
	return alloc_huge_ns_to_ticks(alloc_consts.huge.threshold_ns);
}


uint64_t
alloc_huge_node_deadline_tsc(
	const alloc_huge_node_t* node
	)
{
	return alloc_huge_add_sat_u64(node->time_of_free, alloc_huge_threshold_tsc());
}


alloc_t
alloc_huge_hash(
	alloc_t size
	)
{
	return ((uint64_t) 0x9e3779b97f4a7c15 * size) >> (64 - alloc_consts.huge.slots_shift);
}


alloc_huge_node_t*
alloc_huge_node_get(
	alloc_numa_local_data_t* local
	)
{
	alloc_huge_node_t* node = local->huge.free_node;
	if(!node)
	{
		return NULL;
	}

	local->huge.free_node = node->next;
	node->next = NULL;

	return node;
}


void
alloc_huge_node_ret(
	alloc_numa_local_data_t* local,
	alloc_huge_node_t* node
	)
{
	node->next = local->huge.free_node;
	local->huge.free_node = node;
}


void
alloc_huge_ht_insert(
	alloc_numa_local_data_t* local,
	alloc_huge_node_t* node
	)
{
	alloc_t bucket = alloc_huge_hash(node->size);
	node->next = local->huge.ht[bucket];
	local->huge.ht[bucket] = node;
}


alloc_huge_node_t*
alloc_huge_ht_pop(
	alloc_numa_local_data_t* local,
	alloc_t size
	)
{
	alloc_t bucket = alloc_huge_hash(size);
	alloc_huge_node_t* prev = NULL;
	alloc_huge_node_t* node = local->huge.ht[bucket];

	while(node)
	{
		if(node->size == size)
		{
			if(prev)
			{
				prev->next = node->next;
			}
			else
			{
				local->huge.ht[bucket] = node->next;
			}

			node->next = NULL;
			return node;
		}

		prev = node;
		node = node->next;
	}

	return NULL;
}


void
alloc_huge_ht_remove(
	alloc_numa_local_data_t* local,
	alloc_huge_node_t* node
	)
{
	alloc_t bucket = alloc_huge_hash(node->size);
	alloc_huge_node_t* prev = NULL;
	alloc_huge_node_t* cur = local->huge.ht[bucket];

	while(cur)
	{
		if(cur == node)
		{
			if(prev)
			{
				prev->next = cur->next;
			}
			else
			{
				local->huge.ht[bucket] = cur->next;
			}

			cur->next = NULL;
			return;
		}

		prev = cur;
		cur = cur->next;
	}
}


void
alloc_huge_time_unlink(
	alloc_numa_local_data_t* local,
	alloc_huge_node_t* node
	)
{
	if(node->time_prev)
	{
		node->time_prev->time_next = node->time_next;
	}
	else
	{
		local->huge.time_head = node->time_next;
	}

	if(node->time_next)
	{
		node->time_next->time_prev = node->time_prev;
	}
	else
	{
		local->huge.time_tail = node->time_prev;
	}
}


void
alloc_huge_time_link(
	alloc_numa_local_data_t* local,
	alloc_huge_node_t* node
	)
{
	node->time_prev = local->huge.time_tail;
	node->time_next = NULL;

	if(local->huge.time_tail)
	{
		local->huge.time_tail->time_next = node;
	}
	else
	{
		local->huge.time_head = node;
	}

	local->huge.time_tail = node;
}


void
alloc_huge_time_update_timer(
	alloc_numa_local_data_t* local
	)
{
	if(attr_unlikely(!atomic_load_acq(&alloc_huge.calibrated)))
	{
		atomic_store_rx(&local->huge.timer, alloc_huge.calibrate_deadline_tsc);
		return;
	}

	if(!local->huge.time_head)
	{
		atomic_store_rx(&local->huge.timer, UINT64_MAX);
		return;
	}

	atomic_store_rx(&local->huge.timer, alloc_huge_node_deadline_tsc(local->huge.time_head));
}


void
alloc_huge_reap_locked(
	alloc_numa_local_data_t* local,
	uint64_t now_tsc
	)
{
	while(local->huge.time_head)
	{
		alloc_huge_node_t* head = local->huge.time_head;
		uint64_t deadline = alloc_huge_node_deadline_tsc(head);

		if(now_tsc < deadline)
		{
			atomic_store_rx(&local->huge.timer, deadline);
			return;
		}

		alloc_huge_time_unlink(local, head);
		alloc_huge_ht_remove(local, head);

		void* ptr = head->ptr;
		alloc_t size = head->size;
		alloc_huge_node_ret(local, head);
		alloc_free_virtual_e(ptr, size);
	}

	alloc_huge_time_update_timer(local);
}


void
alloc_huge_init(
	alloc_numa_local_data_t* local
	)
{
	if(!atomic_load_acq(&alloc_huge.initialized))
	{
		alloc_huge_global_init();
	}

	sync_mtx_init(&local->huge.mtx);

	local->huge.free_node = alloc_consts.huge.slots ? local->huge.nodes : NULL;
	local->huge.time_head = NULL;
	local->huge.time_tail = NULL;

	uint64_t timer = atomic_load_acq(&alloc_huge.calibrated) ? UINT64_MAX : alloc_huge.calibrate_deadline_tsc;
	atomic_store_rx(&local->huge.timer, timer);

	memset(local->huge.ht, 0, sizeof(*local->huge.ht) * alloc_consts.huge.slots);

	for(alloc_t i = 0; i < alloc_consts.huge.slots; ++i)
	{
		local->huge.nodes[i].next = i + 1 < alloc_consts.huge.slots ? &local->huge.nodes[i + 1] : NULL;
		local->huge.nodes[i].time_prev = NULL;
		local->huge.nodes[i].time_next = NULL;
		local->huge.nodes[i].ptr = NULL;
		local->huge.nodes[i].size = 0;
		local->huge.nodes[i].time_of_free = 0;
	}
}


void
alloc_huge_reap_due(
	uint16_t numa,
	uint64_t now_tsc
	)
{
	alloc_numa_local_data_t* local = alloc_get_numa_local_data(numa);

	if(attr_likely(now_tsc < atomic_load_rx(&local->huge.timer)))
	{
		return;
	}

	sync_mtx_lock(&local->huge.mtx);

	if(attr_unlikely(now_tsc >= atomic_load_rx(&local->huge.timer)))
	{
		alloc_huge_try_calibrate();
		alloc_huge_reap_locked(local, now_tsc);
	}

	sync_mtx_unlock(&local->huge.mtx);
}


void*
alloc_huge_alloc(
	alloc_t size,
	int zero,
	uint16_t numa
	)
{
	alloc_numa_local_data_t* local = alloc_get_numa_local_data(numa);

	sync_mtx_lock(&local->huge.mtx);

	alloc_huge_node_t* node = alloc_huge_ht_pop(local, size);
	void* result = NULL;

	if(node)
	{
		result = node->ptr;
		alloc_huge_time_unlink(local, node);
		alloc_huge_node_ret(local, node);
		alloc_huge_time_update_timer(local);
	}

	sync_mtx_unlock(&local->huge.mtx);

	if(result)
	{
		if(zero)
		{
			memset(result, 0, size);
		}

		alloc_report_virtual_alloc(size);

		return result;
	}

	result = alloc_alloc_virtual_e(size, size, local->numa, 1);
	if(result)
	{
		alloc_report_virtual_alloc(size);
	}

	return result;
}


void
alloc_huge_free(
	const volatile void* ptr,
	alloc_t size,
	uint16_t numa
	)
{
	alloc_numa_local_data_t* local = alloc_get_numa_local_data(numa);
	alloc_report_virtual_free(size);

	sync_mtx_lock(&local->huge.mtx);

	alloc_huge_node_t* node = alloc_huge_node_get(local);
	if(attr_unlikely(!node))
	{
		sync_mtx_unlock(&local->huge.mtx);
		alloc_free_virtual_e(ptr, size);
		return;
	}

	node->ptr = (void*) ptr;
	node->size = size;
	node->time_of_free = alloc_huge_read_tsc();

	alloc_huge_time_link(local, node);
	alloc_huge_ht_insert(local, node);
	alloc_huge_time_update_timer(local);

	sync_mtx_unlock(&local->huge.mtx);
}

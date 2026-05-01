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
#include <alloc/attr.h>
#include <alloc/sync.h>
#include <alloc/debug.h>
#include <alloc/macro.h>
#include <alloc/types.h>
#include <alloc/atomic.h>
#include <alloc/consts.h>
#include <alloc/report.h>
#include <alloc/threads.h>

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#define ALLOC_REPORT_LOG_TAG "[alloc/report]"
#define alloc_report_log_info(...) alloc_do_custom_log_tagged_info(alloc_consts.report.fd, ALLOC_REPORT_LOG_TAG, __VA_ARGS__)
#define alloc_report_log_warning(...) alloc_do_custom_log_tagged_warning(alloc_consts.report.fd, ALLOC_REPORT_LOG_TAG, __VA_ARGS__)

#define alloc_report_append_typed(dst, capacity, used, ...)				\
do																		\
{																		\
	char _fmt_buf[4096];												\
	alloc_log_build_fmt(_fmt_buf, sizeof(_fmt_buf),						\
		MACRO_COUNT(__VA_ARGS__), MACRO_FORMAT(__VA_ARGS__));			\
	alloc_report_append(dst, capacity, used, _fmt_buf, __VA_ARGS__);	\
}																		\
while(0)


typedef struct alloc_report_size_count
{
	alloc_t size;
	alloc_t count;
}
alloc_report_size_count_t;


typedef struct alloc_report_thread_counts
{
	alloc_tid_t tid;
	alloc_t used;
	alloc_t total;
	alloc_report_size_count_t by_size[ALLOC_MAX_HANDLE_COUNT];
}
alloc_report_thread_counts_t;


typedef struct alloc_report_virtual_count
{
	alloc_t size;
	alloc_t count;
}
alloc_report_virtual_count_t;


typedef struct alloc_report_memory_stats
{
	struct
	{
		alloc_t reserved_bytes;
		alloc_t committed_bytes;
		alloc_t live_slot_bytes;
		alloc_t bitmap_bytes;
	}
	arena;

	struct
	{
		alloc_t capacity_bytes;
		alloc_t committed_bytes;
		alloc_t live_bytes;
	}
	tcb;

	struct
	{
		alloc_t data_reserved_bytes;
		alloc_t data_committed_bytes;
		alloc_t metadata_virtual_bytes;
	}
	numa;

	struct
	{
		alloc_t nonvirtual_allocs;
		alloc_t nonvirtual_bytes;
		alloc_t virtual_allocs;
		alloc_t virtual_bytes;
	}
	live;

	struct
	{
		alloc_t committed_nonvirtual_bytes;
		alloc_t reserved_nonvirtual_bytes;
		alloc_t handled_live_bytes;
	}
	allocator;
}
alloc_report_memory_stats_t;


typedef struct alloc_report
{
	thread_t thread;
	int _Atomic started;
	int _Atomic stop_requested;

	struct
	{
		sync_mtx_t mtx;
		alloc_report_virtual_count_t counts[ALLOC_REPORT_HUGE_SLOTS];
		alloc_t used;
		alloc_t untracked_frees;
	}
	virtual;
}
alloc_report_t;


alloc_report_t alloc_report;


void
alloc_report_init(
	void
	)
{
	sync_mtx_init(&alloc_report.virtual.mtx);
}


alloc_arena_header_t*
alloc_report_slot_to_arena(
	const alloc_numa_local_data_t* local_data,
	alloc_t slot
	)
{
	void* prefix_base = local_data->arena.pool_first_slab + alloc_consts.arena.prefix_region_offset;
	return prefix_base + slot * alloc_consts.arena.prefix_size;
}


void
alloc_report_vappend(
	char* dst,
	size_t capacity,
	size_t* used,
	const char* format,
	va_list args
	)
{
	assert_not_null(dst);
	assert_not_null(used);
	assert_not_null(format);

	if(!capacity)
	{
		return;
	}

	if(*used >= capacity)
	{
		dst[capacity - 1] = 0;
		return;
	}

	int written = vsnprintf(dst + *used, capacity - *used, format, args);

	if(written < 0)
	{
		return;
	}

	size_t appended = written;
	if(appended >= capacity - *used)
	{
		*used = capacity - 1;
		dst[*used] = 0;
		return;
	}

	*used += appended;
}


void
alloc_report_append(
	char* dst,
	size_t capacity,
	size_t* used,
	const char* format,
	...
	)
{
	va_list args;
	va_start(args, format);
		alloc_report_vappend(dst, capacity, used, format, args);
	va_end(args);
}


bool
alloc_report_add_size_count(
	alloc_report_size_count_t* entries,
	alloc_t* used,
	alloc_t capacity,
	alloc_t size,
	alloc_t count
	)
{
	assert_not_null(entries);
	assert_not_null(used);

	alloc_report_size_count_t* entry = entries;
	alloc_report_size_count_t* entry_end = entry + *used;

	while(entry < entry_end)
	{
		if(entry->size == size)
		{
			entry->count += count;
			return true;
		}

		++entry;
	}

	if(*used >= capacity)
	{
		return false;
	}

	++*used;
	*entry =
	(alloc_report_size_count_t)
	{
		.size = size,
		.count = count
	};

	return true;
}


void
alloc_report_sort_size_counts(
	alloc_report_size_count_t* entries,
	alloc_t used
	)
{
	for(alloc_t i = 1; i < used; ++i)
	{
		alloc_report_size_count_t key = entries[i];
		alloc_t j = i;

		while(j && entries[j - 1].size > key.size)
		{
			entries[j] = entries[j - 1];
			--j;
		}

		entries[j] = key;
	}
}


alloc_report_thread_counts_t*
alloc_report_get_thread_counts(
	alloc_report_thread_counts_t* entries,
	alloc_t* used,
	alloc_tid_t tid,
	int* overflow
	)
{
	assert_not_null(entries);
	assert_not_null(used);
	assert_not_null(overflow);

	alloc_report_thread_counts_t* entry = entries;
	alloc_report_thread_counts_t* entry_end = entry + *used;

	while(entry < entry_end)
	{
		if(entry->tid == tid)
		{
			return entry;
		}

		++entry;
	}

	if(*used >= ALLOC_MAX_REPORT_THREADS)
	{
		*overflow = 1;
		return NULL;
	}

	++*used;
	*entry =
	(alloc_report_thread_counts_t)
	{
		.tid = tid
	};

	return entry;
}


void
alloc_report_collect_slab_counts(
	alloc_report_size_count_t* global_entries,
	alloc_t* global_used,
	int* global_overflow,
	alloc_report_thread_counts_t* thread_entries,
	alloc_t* thread_used,
	int* thread_overflow,
	alloc_t* out_nonvirtual_live_allocs,
	alloc_t* out_nonvirtual_live_bytes
	)
{
	assert_not_null(global_entries);
	assert_not_null(global_used);
	assert_not_null(global_overflow);

	int per_thread = alloc_consts.report.per_thread_enable;

	for(alloc_numa_t numa = 0; numa < alloc_consts.numa.valid; ++numa)
	{
		alloc_numa_local_data_t* local_data = alloc_get_numa_local_data(numa);

		sync_mtx_lock(&local_data->arena.mtx);

		for(alloc_t slot = 0; slot < local_data->arena.high_watermark; ++slot)
		{
			alloc_t l0_idx = slot >> 6;
			alloc_t l0_bit = slot & 63;
			if(local_data->arena.bitmap_l0[l0_idx] & ((uint64_t) 1 << l0_bit))
			{
				continue;
			}

			alloc_arena_header_t* arena = alloc_report_slot_to_arena(local_data, slot);
			alloc_tid_t tid = atomic_load_acq(&arena->tid);
			alloc_slab_header_t* slab_base = (void*) arena + alloc_consts.arena.slab_indexable_offset;

			for(alloc_slab_idx_t slab_idx = alloc_consts.arena.prefix_slabs;
				slab_idx < alloc_consts.arena.slab_count; ++slab_idx)
			{
				alloc_slab_header_t* slab = slab_base + slab_idx;
				alloc_slab_idx_t span = MACRO_MAX(slab->span, 1);

				if(slab->idx != slab_idx + 1 || !slab->count || !slab->handle)
				{
					slab_idx += span - 1;
					continue;
				}

				alloc_t size = slab->alloc_size;
				alloc_t count = slab->count;

				if(out_nonvirtual_live_allocs)
				{
					*out_nonvirtual_live_allocs += count;
				}
				if(out_nonvirtual_live_bytes)
				{
					*out_nonvirtual_live_bytes += size * count;
				}

				if(!alloc_report_add_size_count(global_entries, global_used,
					ALLOC_MAX_HANDLE_COUNT + ALLOC_REPORT_HUGE_SLOTS, size, count))
				{
					*global_overflow = 1;
				}

				if(per_thread && tid != ALLOC_DEAD_THREAD_TID)
				{
					alloc_report_thread_counts_t* thread =
						alloc_report_get_thread_counts(thread_entries, thread_used, tid, thread_overflow);

					if(thread)
					{
						if(!alloc_report_add_size_count(thread->by_size,
							&thread->used, ALLOC_MAX_HANDLE_COUNT, size, count))
						{
							*thread_overflow = 1;
						}

						thread->total += count;
					}
				}

				slab_idx += span - 1;
		}
		}

		sync_mtx_unlock(&local_data->arena.mtx);
	}
}


void
alloc_report_collect_virtual_counts(
	alloc_report_size_count_t* global_entries,
	alloc_t* global_used,
	int* global_overflow,
	alloc_t* untracked_frees,
	alloc_t* out_virtual_live_allocs,
	alloc_t* out_virtual_live_bytes
	)
{
	assert_not_null(global_entries);
	assert_not_null(global_used);
	assert_not_null(global_overflow);
	assert_not_null(untracked_frees);

	sync_mtx_lock(&alloc_report.virtual.mtx);

	alloc_report_virtual_count_t* count = alloc_report.virtual.counts;
	alloc_report_virtual_count_t* count_end = count + alloc_report.virtual.used;

	while(count < count_end)
	{
		if(!count->count)
		{
			++count;
			continue;
		}

		if(!alloc_report_add_size_count(global_entries, global_used,
			ALLOC_MAX_HANDLE_COUNT + ALLOC_REPORT_HUGE_SLOTS, count->size, count->count))
		{
			*global_overflow = 1;
		}

		if(out_virtual_live_allocs)
		{
			*out_virtual_live_allocs += count->count;
		}
		if(out_virtual_live_bytes)
		{
			*out_virtual_live_bytes += count->size * count->count;
		}

		++count;
	}

	*untracked_frees = alloc_report.virtual.untracked_frees;

	sync_mtx_unlock(&alloc_report.virtual.mtx);
}


alloc_report_memory_stats_t
alloc_report_collect_memory_stats(
	alloc_t nonvirtual_live_allocs,
	alloc_t nonvirtual_live_bytes,
	alloc_t virtual_live_allocs,
	alloc_t virtual_live_bytes
	)
{
	alloc_report_memory_stats_t stats =
	{
		.live =
		{
			.nonvirtual_allocs = nonvirtual_live_allocs,
			.nonvirtual_bytes = nonvirtual_live_bytes,
			.virtual_allocs = virtual_live_allocs,
			.virtual_bytes = virtual_live_bytes
		}
	};

	alloc_t arena_size = alloc_consts.arena.size;
	alloc_t arena_bitmap_words = alloc_consts.numa.arena_bitmap_l0_size + alloc_consts.numa.arena_bitmap_l1_size;
	alloc_t tcb_capacity = alloc_consts.tcb.capacity;
	alloc_t numa_data_size = alloc_consts.numa.data_size;
	uint16_t numa_nodes = alloc_consts.numa.nodes;
	alloc_numa_t numa_valid = alloc_consts.numa.valid;

	for(alloc_numa_t numa = 0; numa < alloc_consts.numa.valid; ++numa)
	{
		alloc_numa_local_data_t* local_data = alloc_get_numa_local_data(numa);

		sync_mtx_lock(&local_data->arena.mtx);
			alloc_t committed_slots = local_data->arena.committed_slots;
			alloc_t used_slots = local_data->arena.used_slots;
			alloc_t pool_region_size = local_data->arena.pool_region_size;
			alloc_t prefix_committed_slots = local_data->arena.prefix_committed_slots;

			stats.arena.reserved_bytes += pool_region_size;
			stats.arena.committed_bytes += committed_slots * arena_size;
			if(prefix_committed_slots)
			{
				void* prefix_raw_base = local_data->arena.pool_first_slab + alloc_consts.arena.prefix_region_offset;
				void* prefix_raw_end = prefix_raw_base + prefix_committed_slots * alloc_consts.arena.prefix_size;
				void* prefix_commit_end = MACRO_ALIGN_UP(prefix_raw_end, alloc_consts.page.mask);
				stats.arena.committed_bytes += prefix_commit_end - prefix_raw_base;
			}

			stats.arena.live_slot_bytes += used_slots * arena_size;
			stats.arena.bitmap_bytes += sizeof(uint64_t) * arena_bitmap_words;
		sync_mtx_unlock(&local_data->arena.mtx);

		sync_mtx_lock(&local_data->tcb.mtx);
			alloc_t free_count = 0;
			for(alloc_tcb_t* free = local_data->tcb.free; free; free = free->next_free)
			{
				++free_count;
			}

			alloc_t live_count = local_data->tcb.count - free_count;
			alloc_t tcb_committed = local_data->tcb.committed;

			stats.tcb.capacity_bytes += sizeof(alloc_tcb_t) * tcb_capacity;
			stats.tcb.committed_bytes += sizeof(alloc_tcb_t) * tcb_committed;
			stats.tcb.live_bytes += sizeof(alloc_tcb_t) * live_count;

			alloc_t local_data_committed = sizeof(alloc_numa_local_data_t) + sizeof(alloc_tcb_t) * tcb_committed;
			local_data_committed = MACRO_ALIGN_UP(local_data_committed, alloc_consts.page.mask);

			stats.numa.data_committed_bytes += local_data_committed;
		sync_mtx_unlock(&local_data->tcb.mtx);
	}

	stats.numa.data_reserved_bytes = numa_data_size * numa_valid;

	stats.numa.metadata_virtual_bytes = sizeof(*alloc_consts.numa.map) * numa_nodes
		+ sizeof(*alloc_consts.numa.reverse_map) * numa_nodes;

	if(alloc_consts.numa.nodes > 1)
	{
		stats.numa.metadata_virtual_bytes += sizeof(*alloc_consts.numa.data.multiple) * numa_valid;
	}

	stats.allocator.committed_nonvirtual_bytes = stats.arena.committed_bytes
		+ stats.numa.data_committed_bytes + stats.arena.bitmap_bytes + stats.numa.metadata_virtual_bytes;

	stats.allocator.reserved_nonvirtual_bytes = stats.arena.reserved_bytes
		+ stats.numa.data_reserved_bytes + stats.arena.bitmap_bytes + stats.numa.metadata_virtual_bytes;

	stats.allocator.handled_live_bytes = stats.live.nonvirtual_bytes + stats.live.virtual_bytes;

	return stats;
}


void
alloc_report_emit(
	void
	)
{
	alloc_report_size_count_t global_entries[ALLOC_MAX_HANDLE_COUNT + ALLOC_REPORT_HUGE_SLOTS] = {0};
	alloc_t global_used = 0;
	int global_overflow = 0;

	alloc_report_thread_counts_t thread_entries[ALLOC_MAX_REPORT_THREADS] = {0};
	alloc_t thread_used = 0;
	int thread_overflow = 0;
	alloc_t nonvirtual_live_allocs = 0;
	alloc_t nonvirtual_live_bytes = 0;

	alloc_report_collect_slab_counts(global_entries, &global_used, &global_overflow,
		thread_entries, &thread_used, &thread_overflow, &nonvirtual_live_allocs, &nonvirtual_live_bytes);

	alloc_t untracked_frees = 0;
	alloc_t virtual_live_allocs = 0;
	alloc_t virtual_live_bytes = 0;

	alloc_report_collect_virtual_counts(global_entries, &global_used,
		&global_overflow, &untracked_frees, &virtual_live_allocs, &virtual_live_bytes);

	alloc_report_memory_stats_t mem = alloc_report_collect_memory_stats(
		nonvirtual_live_allocs, nonvirtual_live_bytes, virtual_live_allocs, virtual_live_bytes);

	alloc_report_sort_size_counts(global_entries, global_used);

	alloc_report_size_count_t* entry = global_entries;
	alloc_report_size_count_t* entry_end = entry + global_used;

	alloc_t total = 0;

	while(entry < entry_end)
	{
		total += entry->count;
		++entry;
	}

	char msg[65536];
	size_t used = 0;
	msg[0] = 0;

	alloc_report_append_typed(msg, sizeof(msg), &used,
		"snapshot ="
		"\n{"
		"\n\t.total_live = ", total, ","
		"\n\t.classes = ", global_used, ","
		"\n\t.global ="
		"\n\t{");

	entry = global_entries;

	while(entry < entry_end)
	{
		alloc_report_append_typed(msg, sizeof(msg), &used,
			"\n\t\t[", entry->size, "] = ", entry->count, ",");
		++entry;
	}

	alloc_report_append_typed(msg, sizeof(msg), &used, "\n\t},");

	if(global_overflow)
	{
		alloc_report_append_typed(msg, sizeof(msg), &used,
			"\n\t.global_overflow = 1,"
			"\n\t.global_capacity = ", ALLOC_MAX_HANDLE_COUNT + ALLOC_REPORT_HUGE_SLOTS, ",");
	}

	if(untracked_frees)
	{
		alloc_report_append_typed(msg, sizeof(msg), &used,
			"\n\t.virtual_untracked_frees = ", untracked_frees, ",");
	}

	alloc_report_append_typed(msg, sizeof(msg), &used,
		"\n\t.memory ="
		"\n\t{"
		"\n\t\t.arena_reserved_bytes = ", mem.arena.reserved_bytes, ","
		"\n\t\t.arena_committed_bytes = ", mem.arena.committed_bytes, ","
		"\n\t\t.arena_live_slot_bytes = ", mem.arena.live_slot_bytes, ","
		"\n\t\t.arena_bitmap_bytes = ", mem.arena.bitmap_bytes, ","
		"\n\t\t.tcb_capacity_bytes = ", mem.tcb.capacity_bytes, ","
		"\n\t\t.tcb_committed_bytes = ", mem.tcb.committed_bytes, ","
		"\n\t\t.tcb_live_bytes = ", mem.tcb.live_bytes, ","
		"\n\t\t.numa_data_reserved_bytes = ", mem.numa.data_reserved_bytes, ","
		"\n\t\t.numa_data_committed_bytes = ", mem.numa.data_committed_bytes, ","
		"\n\t\t.numa_metadata_virtual_bytes = ", mem.numa.metadata_virtual_bytes, ","
		"\n\t\t.allocator_internal_committed_nonvirtual_bytes = ", mem.allocator.committed_nonvirtual_bytes, ","
		"\n\t\t.allocator_internal_reserved_nonvirtual_bytes = ", mem.allocator.reserved_nonvirtual_bytes, ","
		"\n\t\t.nonvirtual_live_allocs = ", mem.live.nonvirtual_allocs, ","
		"\n\t\t.nonvirtual_live_bytes = ", mem.live.nonvirtual_bytes, ","
		"\n\t\t.virtual_live_allocs = ", mem.live.virtual_allocs, ","
		"\n\t\t.virtual_live_bytes = ", mem.live.virtual_bytes, ","
		"\n\t\t.total_live_allocs_handled = ", mem.live.nonvirtual_allocs + mem.live.virtual_allocs, ","
		"\n\t\t.total_live_bytes_handled = ", mem.allocator.handled_live_bytes,
		"\n\t},");

	if(!alloc_consts.report.per_thread_enable)
	{
		alloc_report_append_typed(msg, sizeof(msg), &used, "\n\t.per_thread_enable = 0\n}");
		alloc_report_log_info(msg);
		return;
	}

	alloc_report_append_typed(msg, sizeof(msg), &used, "\n\t.per_thread =\n\t{");

	for(alloc_t i = 0; i < thread_used; ++i)
	{
		alloc_report_sort_size_counts(thread_entries[i].by_size, thread_entries[i].used);
		alloc_report_append_typed(msg, sizeof(msg), &used,
			"\n\t\t.tid_", thread_entries[i].tid, " ="
			"\n\t\t{"
			"\n\t\t\t.total_live = ", thread_entries[i].total, ","
			"\n\t\t\t.classes = ", thread_entries[i].used, ","
			"\n\t\t\t.sizes ="
			"\n\t\t\t{");

		for(alloc_t j = 0; j < thread_entries[i].used; ++j)
		{
			alloc_report_append_typed(msg, sizeof(msg), &used,
				"\n\t\t\t\t[", thread_entries[i].by_size[j].size, "] = ", thread_entries[i].by_size[j].count, ",");
		}

		alloc_report_append_typed(msg, sizeof(msg), &used, "\n\t\t\t}\n\t\t},");
	}

	alloc_report_append_typed(msg, sizeof(msg), &used, "\n\t}");

	if(thread_overflow)
	{
		alloc_report_append_typed(msg, sizeof(msg), &used, ","
			"\n\t.thread_overflow = 1,"
			"\n\t.thread_capacity = ", ALLOC_MAX_REPORT_THREADS);
	}

	alloc_report_append_typed(msg, sizeof(msg), &used, "\n}");
	alloc_report_log_info(msg);
}


void
alloc_report_thread_fn(
	void* arg
	)
{
	(void) arg;

	while(!atomic_load_acq(&alloc_report.stop_requested))
	{
		if(alloc_consts.report.enable)
		{
			alloc_report_emit();
		}

		struct timespec sleep_time =
		{
			.tv_sec = alloc_consts.report.period_ms / 1000,
			.tv_nsec = (alloc_consts.report.period_ms % 1000) * 1000000
		};

		while(nanosleep(&sleep_time, &sleep_time) && errno == EINTR && !atomic_load_acq(&alloc_report.stop_requested));
	}
}


void
alloc_report_start(
	void
	)
{
	if(atomic_exchange_acq_rel(&alloc_report.started, 1))
	{
		return;
	}

	atomic_store_rel(&alloc_report.stop_requested, 0);

	thread_init(&alloc_report.thread, (thread_data_t){ alloc_report_thread_fn, NULL });
}


void
alloc_report_stop(
	void
	)
{
	if(!atomic_exchange_acq_rel(&alloc_report.started, 0))
	{
		return;
	}

	atomic_store_rel(&alloc_report.stop_requested, 1);

	thread_join(alloc_report.thread);
}


void
alloc_report_refresh(
	void
	)
{
	if(alloc_consts.report.enable)
	{
		alloc_report_start();
		return;
	}

	alloc_report_stop();
}


void
alloc_report_virtual_alloc(
	alloc_t size
	)
{
	if(!alloc_consts.report.enable)
	{
		return;
	}

	sync_mtx_lock(&alloc_report.virtual.mtx);

	for(alloc_t i = 0; i < alloc_report.virtual.used; ++i)
	{
		if(alloc_report.virtual.counts[i].size == size)
		{
			++alloc_report.virtual.counts[i].count;
			sync_mtx_unlock(&alloc_report.virtual.mtx);
			return;
		}
	}

	if(alloc_report.virtual.used < ALLOC_REPORT_HUGE_SLOTS)
	{
		alloc_report.virtual.counts[alloc_report.virtual.used++] =
		(alloc_report_virtual_count_t)
		{
			.size = size,
			.count = 1
		};
	}

	sync_mtx_unlock(&alloc_report.virtual.mtx);
}


void
alloc_report_virtual_free(
	alloc_t size
	)
{
	if(attr_likely(!alloc_consts.report.enable))
	{
		return;
	}

	sync_mtx_lock(&alloc_report.virtual.mtx);

	for(alloc_t i = 0; i < alloc_report.virtual.used; ++i)
	{
		if(alloc_report.virtual.counts[i].size == size && alloc_report.virtual.counts[i].count)
		{
			--alloc_report.virtual.counts[i].count;
			sync_mtx_unlock(&alloc_report.virtual.mtx);
			return;
		}
	}

	++alloc_report.virtual.untracked_frees;

	sync_mtx_unlock(&alloc_report.virtual.mtx);
}

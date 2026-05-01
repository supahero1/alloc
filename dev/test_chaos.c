#include <alloc/log.h>
#include <alloc/base.h>
#include <alloc/test.h>
#include <alloc/atomic.h>
#include <alloc/threads.h>
#include <alloc/platform.h>

#include <stdio.h>
#include <signal.h>

#define CHAOS_LOG_TAG "[alloc/chaos]"
#define chaos_log_info(...) alloc_do_custom_log_tagged_info(STDERR_FILENO, CHAOS_LOG_TAG, __VA_ARGS__)
#define chaos_log_error(...) alloc_do_custom_log_tagged_error(STDERR_FILENO, CHAOS_LOG_TAG, __VA_ARGS__)

#define CHAOS_TRACE_CAP 64
#define CHAOS_NS_PER_SECOND 1000000000
#define CHAOS_CRASH_EXIT_BASE 128
#define CHAOS_MIB_SHIFT 20

#define CHAOS_WORKERS_CAP 64
#define CHAOS_LOCAL_CAP 1024
#define CHAOS_FOREIGN_QUEUE_CAP 32768
#define CHAOS_FOREIGN_DRAIN_BATCH_DEFAULT 8
#define CHAOS_FOREIGN_DRAIN_BATCH_MAX 64
#define CHAOS_SLEEP_MIN_US_DEFAULT 200
#define CHAOS_SLEEP_JITTER_US_DEFAULT 800

#define CHAOS_NEAR_CUTOFF_PERMILL_DEFAULT 1
#define CHAOS_NEAR_CUTOFF_PERMILL_MAX 400
#define CHAOS_VIRTUAL_PERMILL_DEFAULT 2
#define CHAOS_VIRTUAL_PERMILL_MAX 400
#define CHAOS_TOTAL_LOAD_PERMILL_MAX 700
#define CHAOS_PERMILL_BASE 1000
#define CHAOS_OOM_PRESSURE_ADD 32
#define CHAOS_OOM_PRESSURE_DECAY 4
#define CHAOS_OOM_PRESSURE_MAX 100000

#define CHAOS_CPU_HINT_FALLBACK 8
#define CHAOS_WORKERS_MAX_FLOOR 4
#define CHAOS_MIN_WORKER_DIVISOR 4

#define CHAOS_MEM_LIMIT_MB_DEFAULT 1024
#define CHAOS_MEM_SOFT_PERMILL_DEFAULT 900
#define CHAOS_MEM_SOFT_PERMILL_MIN 100
#define CHAOS_MEM_SOFT_PERMILL_MAX 990

#define CHAOS_NEAR_CUTOFF_CLASS_SIZE 4096
#define CHAOS_REALLOC_CLASS_FALLBACK 50688
#define CHAOS_REGULAR_HIGH_MAX 65536

#define CHAOS_ACTION_MODULO 100
#define CHAOS_ACTION_SPAWN_THRESHOLD 45
#define CHAOS_ACTION_STOP_THRESHOLD 80

#define CHAOS_LOCAL_ALLOC_THRESHOLD_NORMAL 35
#define CHAOS_LOCAL_FREE_THRESHOLD_NORMAL 55
#define CHAOS_LOCAL_HANDOFF_THRESHOLD_NORMAL 90
#define CHAOS_LOCAL_ALLOC_THRESHOLD_SOFT 8
#define CHAOS_LOCAL_FREE_THRESHOLD_SOFT 78
#define CHAOS_LOCAL_HANDOFF_THRESHOLD_SOFT 98

#define CHAOS_CONFIG_ARENA_MAX_FREE_PER_THREAD 2
#define CHAOS_CONFIG_SLAB_MAX_FREE_PER_HANDLE 4


typedef struct chaos_alloc
{
	void* ptr;
	alloc_t size;
	alloc_t charge;
	uint32_t owner;
}
chaos_alloc_t;


typedef struct chaos_queue
{
	sync_mtx_t mtx;
	chaos_alloc_t items[CHAOS_FOREIGN_QUEUE_CAP];
	uint32_t head;
	uint32_t tail;
	uint32_t count;
}
chaos_queue_t;


typedef struct chaos_worker
{
	thread_t thread;
	int running;
	int _Atomic stop;

	uint32_t id;
	uint32_t seed;
	chaos_queue_t* foreign;
	chaos_alloc_t local[CHAOS_LOCAL_CAP];
	uint32_t local_count;
	uint64_t alloc_ops;
	uint64_t alloc_fail_ops;
	uint64_t free_ops;
	uint64_t handoff_ops;
	uint64_t foreign_free_ops;
}
chaos_worker_t;


typedef struct chaos_stats
{
	uint64_t alloc_ops;
	uint64_t alloc_fail_ops;
	uint64_t free_ops;
	uint64_t handoff_ops;
	uint64_t foreign_free_ops;
	uint32_t spawn_ops;
	uint32_t stop_ops;
}
chaos_stats_t;


volatile sig_atomic_t chaos_stop_requested;
uint32_t chaos_seed_for_repro;
uint32_t chaos_foreign_drain_batch = CHAOS_FOREIGN_DRAIN_BATCH_DEFAULT;
uint32_t chaos_near_cutoff_permill = CHAOS_NEAR_CUTOFF_PERMILL_DEFAULT;
uint32_t chaos_virtual_permill = CHAOS_VIRTUAL_PERMILL_DEFAULT;
uint64_t _Atomic chaos_live_bytes;
uint64_t _Atomic chaos_live_bytes_peak;
uint64_t chaos_mem_soft_limit_bytes;
uint64_t chaos_mem_hard_limit_bytes;
uint32_t _Atomic chaos_oom_pressure;


uint64_t
chaos_env_u64(
	const char* name,
	uint64_t fallback
	)
{
	const char* raw = getenv(name);
	if(!raw || !*raw)
	{
		return fallback;
	}

	char* end = NULL;
	unsigned long long v = strtoull(raw, &end, 10);
	if(end == raw || *end != '\0')
	{
		return fallback;
	}

	return v;
}


uint64_t
chaos_live_bytes_load(
	void
	)
{
	return atomic_load_acq(&chaos_live_bytes);
}


uint64_t
chaos_live_bytes_peak_load(
	void
	)
{
	return atomic_load_acq(&chaos_live_bytes_peak);
}


void
chaos_live_bytes_track_peak(
	uint64_t live_bytes
	)
{
	uint64_t peak = chaos_live_bytes_peak_load();
	while(live_bytes > peak && !atomic_exchange_weak_acq_rel(&chaos_live_bytes_peak, &peak, live_bytes));
}


void
chaos_live_bytes_add(
	alloc_t size
	)
{
	uint64_t live = atomic_fetch_add_acq_rel(&chaos_live_bytes, size) + size;
	chaos_live_bytes_track_peak(live);
}


void
chaos_live_bytes_sub(
	alloc_t size
	)
{
	uint64_t old = atomic_fetch_sub_acq_rel(&chaos_live_bytes, size);
	assert_ge(old, size);
}


void
chaos_oom_pressure_raise(
	void
	)
{
	uint32_t old = atomic_fetch_add_acq_rel(&chaos_oom_pressure, CHAOS_OOM_PRESSURE_ADD);
	if(old >= CHAOS_OOM_PRESSURE_MAX)
	{
		atomic_store_rel(&chaos_oom_pressure, CHAOS_OOM_PRESSURE_MAX);
	}
}


void
chaos_oom_pressure_decay(
	void
	)
{
	uint32_t old = atomic_load_acq(&chaos_oom_pressure);
	if(!old)
	{
		return;
	}

	uint32_t dec = MACRO_MIN(old, CHAOS_OOM_PRESSURE_DECAY);
	atomic_fetch_sub_acq_rel(&chaos_oom_pressure, dec);
}


uint64_t
chaos_now_monotonic_ns(
	void
	)
{
	return alloc_read_time_ns();
}


uint32_t
chaos_env_u32(
	const char* name,
	uint32_t fallback
	)
{
	const char* raw = getenv(name);
	if(!raw || !*raw)
	{
		return fallback;
	}

	char* end = NULL;
	unsigned long long v = strtoull(raw, &end, 10);
	if(end == raw || *end != '\0')
	{
		return fallback;
	}

	return MACRO_MIN(v, UINT32_MAX);
}


void
chaos_snapshot_running_stats(
	const chaos_worker_t* workers,
	uint32_t worker_slots,
	chaos_stats_t* out
	)
{
	*out = (chaos_stats_t){0};

	const chaos_worker_t* worker = workers;
	const chaos_worker_t* worker_end = worker + worker_slots;

	while(worker < worker_end)
	{
		if(worker->running)
		{
			out->alloc_ops += worker->alloc_ops;
			out->alloc_fail_ops += worker->alloc_fail_ops;
			out->free_ops += worker->free_ops;
			out->handoff_ops += worker->handoff_ops;
			out->foreign_free_ops += worker->foreign_free_ops;
		}

		++worker;
	}
}


void
chaos_stop_signal_handler(
	int signo
	)
{
	(void) signo;

	chaos_stop_requested = 1;
}


void
chaos_crash_signal_handler(
	int signo
	)
{
	chaos_log_error("crash: signal=", signo, " seed=", chaos_seed_for_repro);
	hard_assert_unreachable();
}


int
chaos_queue_push(
	chaos_queue_t* q,
	const chaos_alloc_t* item
	)
{
	int status = false;

	sync_mtx_lock(&q->mtx);
		if(q->count < CHAOS_FOREIGN_QUEUE_CAP)
		{
			q->items[q->tail] = *item;
			q->tail = (q->tail + 1) % CHAOS_FOREIGN_QUEUE_CAP;
			++q->count;
			status = true;
		}
	sync_mtx_unlock(&q->mtx);

	return status;
}


int
chaos_queue_pop(
	chaos_queue_t* q,
	chaos_alloc_t* item
	)
{
	int status = false;

	sync_mtx_lock(&q->mtx);
		if(q->count)
		{
			*item = q->items[q->head];
			q->head = (q->head + 1) % CHAOS_FOREIGN_QUEUE_CAP;
			--q->count;
			status = true;
		}
	sync_mtx_unlock(&q->mtx);

	return status;
}


uint32_t
chaos_queue_count(
	chaos_queue_t* q
	)
{
	sync_mtx_lock(&q->mtx);
		uint32_t count = q->count;
	sync_mtx_unlock(&q->mtx);

	return count;
}


alloc_t
chaos_random_size_core(
	chaos_worker_t* worker
	)
{
	uint32_t r = rand_r(&worker->seed) % 1000;
	int k = 0;

	if(r < 930)
	{
		while(k < 13 && rand_r(&worker->seed) % 100 < 45)
		{
			++k;
		}
	}
	else if(r < 990)
	{
		k = 10 + rand_r(&worker->seed) % 4;
	}
	else
	{
		k = 13;
	}

	alloc_t base = MACRO_MIN((alloc_t) 8 << k, CHAOS_REGULAR_HIGH_MAX);

	if(base <= 512)
	{
		alloc_t next = base << 1;
		alloc_t hi = next - 1;
		return base + rand_r(&worker->seed) % (hi - base + 1);
	}

	if(base <= 4096)
	{
		if(rand_r(&worker->seed) % 100 < 70)
		{
			return base;
		}

		alloc_t next = base << 1;
		alloc_t hi = next - 1;
		return base + rand_r(&worker->seed) % (hi - base + 1);
	}

	if(rand_r(&worker->seed) % 100 < 94)
	{
		return base;
	}

	alloc_t step = MACRO_MAX(base >> 4, 1);
	alloc_t delta = rand_r(&worker->seed) % step;
	return base - delta;
}


alloc_t
chaos_random_size(
	chaos_worker_t* worker
	)
{
	uint32_t r = rand_r(&worker->seed) % CHAOS_PERMILL_BASE;

	if(r < chaos_near_cutoff_permill)
	{
		if(rand_r(&worker->seed) & 1)
		{
			return alloc_consts.arena.size - MACRO_MIN(alloc_consts.arena.size, alloc_consts.page.size);
		}

		if(alloc_consts.slab.max_nonvirtual_size > 1)
		{
			return alloc_consts.slab.max_nonvirtual_size - 1;
		}

		return 1;
	}

	if(r < chaos_near_cutoff_permill + chaos_virtual_permill)
	{
		if(rand_r(&worker->seed) & 1)
		{
			return alloc_consts.slab.max_nonvirtual_size + 1;
		}

		return alloc_consts.slab.max_nonvirtual_size + alloc_consts.page.size;
	}

	return MACRO_MIN(chaos_random_size_core(worker), alloc_consts.slab.max_nonvirtual_size);
}


alloc_t
chaos_charge_size(
	alloc_t requested_size
	)
{
	const alloc_handle_t* handle = alloc_test_get_handle(requested_size);
	if(!handle)
	{
		return requested_size;
	}

	if(handle->alloc_size)
	{
		return handle->alloc_size;
	}

	return requested_size;
}


chaos_alloc_t
chaos_local_take_random(
	chaos_worker_t* worker
	)
{
	assert_gt(worker->local_count, 0);

	uint32_t idx = rand_r(&worker->seed) % worker->local_count;
	chaos_alloc_t item = worker->local[idx];
	worker->local[idx] = worker->local[worker->local_count - 1];
	--worker->local_count;

	return item;
}


void
chaos_free_item(
	chaos_worker_t* worker,
	const chaos_alloc_t* item
	)
{
	alloc_free_e(item->ptr, item->size);
	chaos_live_bytes_sub(item->charge);
	++worker->free_ops;
}

int
chaos_worker_drain_foreign(
	chaos_worker_t* worker
	)
{
	for(uint32_t i = 0; i < chaos_foreign_drain_batch; ++i)
	{
		chaos_alloc_t item;
		if(!chaos_queue_pop(worker->foreign, &item))
		{
			break;
		}

		if(item.owner == worker->id)
		{
			if(worker->local_count < CHAOS_LOCAL_CAP)
			{
				worker->local[worker->local_count++] = item;
				continue;
			}

			if(!chaos_queue_push(worker->foreign, &item))
			{
				chaos_free_item(worker, &item);
			}

			continue;
		}

		chaos_free_item(worker, &item);
		++worker->foreign_free_ops;
	}

	return true;
}


int
chaos_worker_grow_local(
	chaos_worker_t* worker
	)
{
	alloc_t size = chaos_random_size(worker);
	void* ptr = alloc_alloc_e(size, 0);
	if(!ptr)
	{
		++worker->alloc_fail_ops;
		return false;
	}

	alloc_t charge = chaos_charge_size(size);

	worker->local[worker->local_count++] =
	(chaos_alloc_t)
	{
		.ptr = ptr,
		.size = size,
		.charge = charge,
		.owner = worker->id
	};

	chaos_live_bytes_add(charge);
	++worker->alloc_ops;

	return true;
}


int
chaos_worker_realloc_random(
	chaos_worker_t* worker,
	int at_soft_limit
	)
{
	uint32_t idx = rand_r(&worker->seed) % worker->local_count;
	chaos_alloc_t item = worker->local[idx];

	alloc_t new_size = chaos_random_size(worker);
	if(at_soft_limit && new_size > item.size)
	{
		new_size = rand_r(&worker->seed) % item.size + 1;
	}

	void* new_ptr = alloc_realloc_e(item.ptr, item.size, new_size, 0);
	if(!new_ptr)
	{
		++worker->alloc_fail_ops;
		chaos_oom_pressure_raise();
		return false;
	}

	alloc_t old_charge = item.charge;
	alloc_t new_charge = chaos_charge_size(new_size);
	worker->local[idx].ptr = new_ptr;
	worker->local[idx].size = new_size;
	worker->local[idx].charge = new_charge;

	if(new_charge > old_charge)
	{
		chaos_live_bytes_add(new_charge - old_charge);
	}
	else if(old_charge > new_charge)
	{
		chaos_live_bytes_sub(old_charge - new_charge);
	}

	++worker->alloc_ops;
	return true;
}


void
chaos_worker_fn(
	void* arg
	)
{
	chaos_worker_t* worker = arg;
	assert_not_null(worker);

	while(!chaos_stop_requested && !atomic_load_acq(&worker->stop))
	{
		chaos_worker_drain_foreign(worker);

		uint64_t live_bytes = chaos_live_bytes_load();
		uint32_t oom_pressure = atomic_load_acq(&chaos_oom_pressure);
		int at_hard_limit = live_bytes >= chaos_mem_hard_limit_bytes;
		int at_soft_limit = live_bytes >= chaos_mem_soft_limit_bytes;
		int at_oom_pressure = oom_pressure > 0;
		if(at_oom_pressure)
		{
			at_soft_limit = 1;
		}

		if(!worker->local_count)
		{
			if(at_hard_limit)
			{
				continue;
			}

			chaos_worker_grow_local(worker);

			continue;
		}

		if(at_hard_limit)
		{
			chaos_alloc_t item = chaos_local_take_random(worker);
			chaos_free_item(worker, &item);
			continue;
		}

		uint32_t op = rand_r(&worker->seed) % CHAOS_ACTION_MODULO;
		uint32_t alloc_threshold = at_soft_limit ?
			CHAOS_LOCAL_ALLOC_THRESHOLD_SOFT : CHAOS_LOCAL_ALLOC_THRESHOLD_NORMAL;
		uint32_t free_threshold = at_soft_limit ?
			CHAOS_LOCAL_FREE_THRESHOLD_SOFT : CHAOS_LOCAL_FREE_THRESHOLD_NORMAL;
		uint32_t handoff_threshold = at_soft_limit ?
			CHAOS_LOCAL_HANDOFF_THRESHOLD_SOFT : CHAOS_LOCAL_HANDOFF_THRESHOLD_NORMAL;

		if(op < alloc_threshold && worker->local_count < CHAOS_LOCAL_CAP)
		{
			if(!chaos_worker_grow_local(worker))
			{
				chaos_oom_pressure_raise();

				if(worker->local_count)
				{
					chaos_alloc_t item = chaos_local_take_random(worker);
					chaos_free_item(worker, &item);
				}
			}

			continue;
		}

		if(op < free_threshold)
		{
			chaos_alloc_t item = chaos_local_take_random(worker);
			chaos_free_item(worker, &item);

			if(at_oom_pressure)
			{
				chaos_oom_pressure_decay();
			}

			continue;
		}

		if(op < handoff_threshold)
		{
			chaos_alloc_t item = chaos_local_take_random(worker);

			if(!chaos_queue_push(worker->foreign, &item))
			{
				chaos_free_item(worker, &item);

				if(at_oom_pressure)
				{
					chaos_oom_pressure_decay();
				}
			}
			else
			{
				++worker->handoff_ops;
			}

			continue;
		}

		chaos_worker_realloc_random(worker, at_soft_limit);
	}

	for(uint32_t i = 0; i < worker->local_count; ++i)
	{
		chaos_alloc_t item = worker->local[i];
		if(!chaos_queue_push(worker->foreign, &item))
		{
			chaos_free_item(worker, &item);
		}
		else
		{
			++worker->handoff_ops;
		}
	}

	worker->local_count = 0;
}


int
chaos_spawn_one(
	chaos_worker_t* workers,
	uint32_t worker_slots,
	chaos_queue_t* foreign,
	uint32_t* seed
	)
{
	for(uint32_t i = 0; i < worker_slots; ++i)
	{
		if(workers[i].running)
		{
			continue;
		}

		workers[i] =
		(chaos_worker_t)
		{
			.running = 1,
			.stop = 0,
			.id = i,
			.seed = rand_r(seed) ^ (0x9e3779b9 * (i + 1)),
			.foreign = foreign
		};

		thread_init(&workers[i].thread, (thread_data_t){ chaos_worker_fn, &workers[i] });

		return true;
	}

	return false;
}


int
chaos_stop_one(
	chaos_worker_t* workers,
	uint32_t worker_slots,
	uint32_t* seed,
	chaos_stats_t* stats
	)
{
	uint32_t order[CHAOS_WORKERS_CAP];
	for(uint32_t i = 0; i < worker_slots; ++i)
	{
		order[i] = i;
	}

	for(uint32_t i = 0; i < worker_slots; ++i)
	{
		uint32_t j = rand_r(seed) % worker_slots;
		uint32_t tmp = order[i];
		order[i] = order[j];
		order[j] = tmp;
	}

	for(uint32_t n = 0; n < worker_slots; ++n)
	{
		uint32_t idx = order[n];
		if(!workers[idx].running)
		{
			continue;
		}

		atomic_store_rel(&workers[idx].stop, 1);

		thread_join(workers[idx].thread);

		workers[idx].running = 0;
		stats->alloc_ops += workers[idx].alloc_ops;
		stats->alloc_fail_ops += workers[idx].alloc_fail_ops;
		stats->free_ops += workers[idx].free_ops;
		stats->handoff_ops += workers[idx].handoff_ops;
		stats->foreign_free_ops += workers[idx].foreign_free_ops;

		return true;
	}

	return false;
}


uint32_t
chaos_active_count(
	const chaos_worker_t* workers,
	uint32_t worker_slots
	)
{
	uint32_t active = 0;

	for(uint32_t i = 0; i < worker_slots; ++i)
	{
		active += workers[i].running ? 1 : 0;
	}

	return active;
}


int
main(
	void
	)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	signal(SIGINT, chaos_stop_signal_handler);
	signal(SIGTERM, chaos_stop_signal_handler);
	signal(SIGSEGV, chaos_crash_signal_handler);
	signal(SIGABRT, chaos_crash_signal_handler);
	signal(SIGILL, chaos_crash_signal_handler);
	signal(SIGBUS, chaos_crash_signal_handler);
	signal(SIGFPE, chaos_crash_signal_handler);

	alloc_config_t cfg =
	(alloc_config_t)
	{
		.log_verbosity = ALLOC_LOG_VERBOSITY_WARNING,
		.log_verbosity_set = 1,
		.arena_max_free_per_thread = CHAOS_CONFIG_ARENA_MAX_FREE_PER_THREAD,
		.arena_max_free_per_thread_set = 1,
		.slab_max_free_per_handle = CHAOS_CONFIG_SLAB_MAX_FREE_PER_HANDLE,
		.slab_max_free_per_handle_set = 1
	};
	alloc_configure(&cfg);

	uint32_t cpu_hint = alloc_get_cpus();
	if(!cpu_hint)
	{
		cpu_hint = CHAOS_CPU_HINT_FALLBACK;
	}

	uint32_t workers_max = chaos_env_u32("CHAOS_WORKERS_MAX", cpu_hint << 1);
	workers_max = MACRO_CLAMP(workers_max, CHAOS_WORKERS_MAX_FLOOR, CHAOS_WORKERS_CAP);

	uint32_t workers_min = chaos_env_u32("CHAOS_WORKERS_MIN", workers_max / CHAOS_MIN_WORKER_DIVISOR);
	workers_min = MACRO_CLAMP(workers_min, 1, workers_max - 1);

	uint32_t workers_start_default = MACRO_MAX(workers_max >> 1, 1);
	if(workers_start_default <= workers_min)
	{
		workers_start_default = workers_min + 1;
	}

	uint32_t workers_start = chaos_env_u32("CHAOS_WORKERS_START", workers_start_default);
	workers_start = workers_start ? workers_start : workers_start_default;
	workers_start = MACRO_CLAMP(workers_start, workers_min, workers_max);

	chaos_foreign_drain_batch = chaos_env_u32("CHAOS_DRAIN_BATCH", CHAOS_FOREIGN_DRAIN_BATCH_DEFAULT);
	chaos_foreign_drain_batch = MACRO_CLAMP(
		chaos_foreign_drain_batch ? chaos_foreign_drain_batch : CHAOS_FOREIGN_DRAIN_BATCH_DEFAULT,
		1,
		CHAOS_FOREIGN_DRAIN_BATCH_MAX);

	chaos_near_cutoff_permill = chaos_env_u32("CHAOS_NEAR_CUTOFF_PERMILLE", CHAOS_NEAR_CUTOFF_PERMILL_DEFAULT);
	chaos_near_cutoff_permill = MACRO_MIN(chaos_near_cutoff_permill, CHAOS_NEAR_CUTOFF_PERMILL_MAX);

	chaos_virtual_permill = chaos_env_u32("CHAOS_VIRTUAL_PERMILLE", CHAOS_VIRTUAL_PERMILL_DEFAULT);
	chaos_virtual_permill = MACRO_MIN(chaos_virtual_permill, CHAOS_VIRTUAL_PERMILL_MAX);
	chaos_virtual_permill = MACRO_MIN(
		chaos_virtual_permill,
		CHAOS_TOTAL_LOAD_PERMILL_MAX - chaos_near_cutoff_permill);

	uint64_t mem_limit_mb = chaos_env_u64("CHAOS_MEM_LIMIT_MB", CHAOS_MEM_LIMIT_MB_DEFAULT);
	mem_limit_mb = mem_limit_mb ? mem_limit_mb : CHAOS_MEM_LIMIT_MB_DEFAULT;
	mem_limit_mb = MACRO_MIN(mem_limit_mb, UINT64_MAX >> CHAOS_MIB_SHIFT);
	chaos_mem_hard_limit_bytes = mem_limit_mb << CHAOS_MIB_SHIFT;

	uint32_t soft_permille = chaos_env_u32("CHAOS_MEM_SOFT_PERMILLE", CHAOS_MEM_SOFT_PERMILL_DEFAULT);
	soft_permille = MACRO_CLAMP(soft_permille, CHAOS_MEM_SOFT_PERMILL_MIN, CHAOS_MEM_SOFT_PERMILL_MAX);
	chaos_mem_soft_limit_bytes = chaos_mem_hard_limit_bytes / CHAOS_PERMILL_BASE * soft_permille;
	if(chaos_mem_soft_limit_bytes >= chaos_mem_hard_limit_bytes)
	{
		chaos_mem_soft_limit_bytes = chaos_mem_hard_limit_bytes - 1;
	}

	uint32_t sleep_min_us = chaos_env_u32("CHAOS_SLEEP_MIN_US", CHAOS_SLEEP_MIN_US_DEFAULT);
	uint32_t sleep_jitter_us = chaos_env_u32("CHAOS_SLEEP_JITTER_US", CHAOS_SLEEP_JITTER_US_DEFAULT);

	uint32_t seed = chaos_now_monotonic_ns() ^ getpid();
	const char* seed_env = getenv("WB_CHAOS_SEED");
	if(seed_env && *seed_env)
	{
		seed = strtoul(seed_env, NULL, 10);
	}
	chaos_seed_for_repro = seed;

	alloc_t run_seconds = 0;
	const char* seconds_env = getenv("CHAOS_SECONDS");
	if(seconds_env && *seconds_env)
	{
		run_seconds = strtoull(seconds_env, NULL, 10);
	}

	chaos_log_info("start: seed=", seed, " mode=", run_seconds ? "timed" : "infinite", " workers_start=",
		workers_start, " workers_min=", workers_min, " workers_max=", workers_max, " drain_batch=",
		chaos_foreign_drain_batch, " sleep_us=min:", sleep_min_us, " jitter:", sleep_jitter_us,
		" near_cutoff=", chaos_near_cutoff_permill, "/1000", " virtual=", chaos_virtual_permill,
		"/1000", " mem_soft=", chaos_mem_soft_limit_bytes >> CHAOS_MIB_SHIFT, "MiB",
		" mem_hard=", chaos_mem_hard_limit_bytes >> CHAOS_MIB_SHIFT, "MiB");

	chaos_queue_t foreign = {0};
	sync_mtx_init(&foreign.mtx);

	chaos_worker_t workers[CHAOS_WORKERS_CAP] = {0};
	chaos_stats_t stats = {0};
	chaos_stats_t prev_report_total = {0};

	for(uint32_t i = 0; i < workers_start; ++i)
	{
		if(chaos_spawn_one(workers, workers_max, &foreign, &seed))
		{
			++stats.spawn_ops;
		}
	}

	uint64_t start = chaos_now_monotonic_ns();
	uint64_t next_report = start + CHAOS_NS_PER_SECOND;
	uint64_t run_seconds_ns = run_seconds;
	run_seconds_ns *= CHAOS_NS_PER_SECOND;

	while(!chaos_stop_requested)
	{
		uint64_t now = chaos_now_monotonic_ns();
		if(run_seconds && now - start >= run_seconds_ns)
		{
			break;
		}

		uint32_t active = chaos_active_count(workers, workers_max);
		uint32_t action = rand_r(&seed) % CHAOS_ACTION_MODULO;

		if(action < CHAOS_ACTION_SPAWN_THRESHOLD && active < workers_max)
		{
			if(chaos_spawn_one(workers, workers_max, &foreign, &seed))
			{
				++stats.spawn_ops;
			}
		}
		else if(action < CHAOS_ACTION_STOP_THRESHOLD && active > workers_min)
		{
			if(chaos_stop_one(workers, workers_max, &seed, &stats))
			{
				++stats.stop_ops;
			}
		}

		if(now >= next_report)
		{
			chaos_stats_t running = {0};
			chaos_snapshot_running_stats(workers, workers_max, &running);

			chaos_stats_t total =
			(chaos_stats_t)
			{
				.alloc_ops = stats.alloc_ops + running.alloc_ops,
				.alloc_fail_ops = stats.alloc_fail_ops + running.alloc_fail_ops,
				.free_ops = stats.free_ops + running.free_ops,
				.handoff_ops = stats.handoff_ops + running.handoff_ops,
				.foreign_free_ops = stats.foreign_free_ops + running.foreign_free_ops,
				.spawn_ops = stats.spawn_ops,
				.stop_ops = stats.stop_ops
			};

			uint64_t alloc_ps = total.alloc_ops - prev_report_total.alloc_ops;
			uint64_t alloc_fail_ps = total.alloc_fail_ops - prev_report_total.alloc_fail_ops;
			uint64_t free_ps = total.free_ops - prev_report_total.free_ops;
			uint64_t handoff_ps = total.handoff_ops - prev_report_total.handoff_ops;
			uint64_t foreign_free_ps = total.foreign_free_ops - prev_report_total.foreign_free_ops;
			uint64_t live_bytes = chaos_live_bytes_load();
			uint64_t peak_bytes = chaos_live_bytes_peak_load();
			uint32_t oom_pressure = atomic_load_acq(&chaos_oom_pressure);

			chaos_log_info("progress: active=", chaos_active_count(workers, workers_max), " queue=",
				chaos_queue_count(&foreign), " spawn=", stats.spawn_ops, " stop=", stats.stop_ops,
				" live=", live_bytes >> CHAOS_MIB_SHIFT, "MiB", " peak=", peak_bytes >> CHAOS_MIB_SHIFT,
				"MiB", " oom_pressure=", oom_pressure, " alloc/s=", alloc_ps, " alloc_fail/s=", alloc_fail_ps,
				" free/s=", free_ps, " handoff/s=", handoff_ps, " foreign_free/s=", foreign_free_ps);

			prev_report_total = total;
			next_report += CHAOS_NS_PER_SECOND;
		}

		usleep(sleep_min_us + (sleep_jitter_us ? rand_r(&seed) % (sleep_jitter_us + 1) : 0));
	}

	chaos_stop_requested = 1;

	while(chaos_active_count(workers, workers_max))
	{
		if(chaos_stop_one(workers, workers_max, &seed, &stats))
		{
			++stats.stop_ops;
		}
	}

	while(1)
	{
		chaos_alloc_t item;
		if(!chaos_queue_pop(&foreign, &item))
		{
			break;
		}

		alloc_free_e(item.ptr, item.size);
		chaos_live_bytes_sub(item.charge);
		++stats.free_ops;
	}

	sync_mtx_free(&foreign.mtx);

	uint64_t live_bytes_final = chaos_live_bytes_load();
	assert_eq(live_bytes_final, 0);

	chaos_log_info("done: seed=", chaos_seed_for_repro, " alloc=", stats.alloc_ops, " alloc_fail=",
		stats.alloc_fail_ops, " free=", stats.free_ops, " handoff=", stats.handoff_ops, " foreign_free=",
		stats.foreign_free_ops, " live_peak=", chaos_live_bytes_peak_load() >> CHAOS_MIB_SHIFT, "MiB",
		" spawn=", stats.spawn_ops, " stop=", stats.stop_ops);

	return 0;
}

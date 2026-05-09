#include "common.h"

#include <alloc/atomic.h>

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
	#include <sys/wait.h>
	#include <unistd.h>
#endif

#define XTHREAD_ITERS 200000
#define XTHREAD_RING_SIZE 64
#define XTHREAD_RING_MASK (XTHREAD_RING_SIZE - 1)
#define HANDOFF_POSTFREE_ITERS 2048
#define HANDOFF_POSTFREE_BATCH 16384


typedef struct bench_barrier
{
	sync_mtx_t mtx;
	sync_cond_t cv;
	int waiting;
	int target;
	int generation;
}
bench_barrier_t;


void
bench_barrier_init(
	bench_barrier_t* barrier,
	int target
	)
{
	sync_mtx_init(&barrier->mtx);
	sync_cond_init(&barrier->cv);
	barrier->waiting = 0;
	barrier->target = target;
	barrier->generation = 0;
}


void
bench_barrier_free(
	bench_barrier_t* barrier
	)
{
	sync_cond_free(&barrier->cv);
	sync_mtx_free(&barrier->mtx);
}


void
bench_barrier_wait(
	bench_barrier_t* barrier
	)
{
	sync_mtx_lock(&barrier->mtx);
	int generation = barrier->generation;
	++barrier->waiting;

	if(barrier->waiting == barrier->target)
	{
		barrier->waiting = 0;
		++barrier->generation;
		sync_cond_wake(&barrier->cv);
	}
	else
	{
		while(generation == barrier->generation)
		{
			sync_cond_wait(&barrier->cv, &barrier->mtx);
		}
	}

	sync_mtx_unlock(&barrier->mtx);
}


typedef struct xthread_ring_slot
{
	_Atomic(void*) ptr;
	char pad[56];
}
xthread_ring_slot_t;


typedef struct xthread_arg
{
	size_t alloc_size;
	int iters;
	xthread_ring_slot_t* ring;
	uint64_t _Atomic* prod_times;
	uint64_t _Atomic* cons_times;
	bench_barrier_t* barrier;
}
xthread_arg_t;


void
xthread_producer(
	void* arg
	)
{
	xthread_arg_t* a = arg;

	bench_barrier_wait(a->barrier);

	for(int i = 0; i < a->iters; ++i)
	{
		int slot = i & XTHREAD_RING_MASK;

		while(atomic_load_acq(&a->ring[slot].ptr) != NULL)
		{
		}

		uint64_t t0 = get_ns();
		void* p = bench_alloc(a->alloc_size, 0);
		uint64_t t1 = get_ns();

		atomic_store_rx(&a->prod_times[i], t1 - t0);
		atomic_store_rel(&a->ring[slot].ptr, p);
	}
}


void
xthread_consumer(
	void* arg
	)
{
	xthread_arg_t* a = arg;

	bench_barrier_wait(a->barrier);

	for(int i = 0; i < a->iters; ++i)
	{
		int slot = i & XTHREAD_RING_MASK;
		void* p;

		while((p = atomic_load_acq(&a->ring[slot].ptr)) == NULL)
		{
		}

		uint64_t t0 = get_ns();
		bench_free(p, a->alloc_size);
		uint64_t t1 = get_ns();

		atomic_store_rx(&a->cons_times[i], t1 - t0);
		atomic_store_rel(&a->ring[slot].ptr, NULL);
	}
}


void
bench_foreign_free(
	size_t alloc_size
	)
{
	int n = XTHREAD_ITERS;
	xthread_ring_slot_t* ring = calloc(XTHREAD_RING_SIZE, sizeof(*ring));
	uint64_t _Atomic* prod_times = calloc(n, sizeof(*prod_times));
	uint64_t _Atomic* cons_times = calloc(n, sizeof(*cons_times));

	bench_barrier_t barrier;
	bench_barrier_init(&barrier, 2);

	xthread_arg_t arg =
	{
		.alloc_size = alloc_size,
		.iters = n,
		.ring = ring,
		.prod_times = prod_times,
		.cons_times = cons_times,
		.barrier = &barrier,
	};

	thread_t prod;
	thread_t cons;

	thread_init(&prod, (thread_data_t){ xthread_producer, &arg });
	thread_init(&cons, (thread_data_t){ xthread_consumer, &arg });

	thread_join(prod);
	thread_join(cons);

	bench_barrier_free(&barrier);

	uint64_t* alloc_samples = malloc(sizeof(uint64_t) * n);
	uint64_t* free_samples = malloc(sizeof(uint64_t) * n);

	for(int i = 0; i < n; ++i)
	{
		alloc_samples[i] = atomic_load_rx(&prod_times[i]);
		free_samples[i] = atomic_load_rx(&cons_times[i]);
	}

	stats_t sa = compute_stats(alloc_samples, n);
	stats_t sf = compute_stats(free_samples, n);

	char label[64];
	snprintf(label, sizeof(label), "producer alloc (%5zuB)", alloc_size);
	print_stats(label, &sa);
	snprintf(label, sizeof(label), "consumer free (%5zuB)", alloc_size);
	print_stats(label, &sf);

	free(alloc_samples);
	free(free_samples);
	free(ring);
	free(prod_times);
	free(cons_times);
}


typedef struct handoff_postfree_ctx
{
	size_t alloc_size;
	int iters;
	size_t batch_count;
	void** batch;
	uint64_t* samples;
	sync_mtx_t mtx;
	sync_cond_t cv;
	int phase;
}
handoff_postfree_ctx_t;


void
handoff_postfree_producer(
	void* arg
	)
{
	handoff_postfree_ctx_t* ctx = arg;

	for(int i = 0; i < ctx->iters; ++i)
	{
		for(size_t j = 0; j < ctx->batch_count; ++j)
		{
			ctx->batch[j] = bench_alloc(ctx->alloc_size, 0);
		}

		sync_mtx_lock(&ctx->mtx);
		ctx->phase = 1;
		sync_cond_wake(&ctx->cv);
		while(ctx->phase != 0)
		{
			sync_cond_wait(&ctx->cv, &ctx->mtx);
		}
		sync_mtx_unlock(&ctx->mtx);

		uint64_t t0 = get_ns();
		void* p = bench_alloc(ctx->alloc_size, 0);
		uint64_t t1 = get_ns();
		ctx->samples[i] = t1 - t0;

		if(p)
		{
			bench_free(p, ctx->alloc_size);
		}
	}

	sync_mtx_lock(&ctx->mtx);
	ctx->phase = 2;
	sync_cond_wake(&ctx->cv);
	sync_mtx_unlock(&ctx->mtx);
}


void
handoff_postfree_consumer(
	void* arg
	)
{
	handoff_postfree_ctx_t* ctx = arg;

	while(1)
	{
		sync_mtx_lock(&ctx->mtx);
		while(ctx->phase == 0)
		{
			sync_cond_wait(&ctx->cv, &ctx->mtx);
		}

		if(ctx->phase == 2)
		{
			sync_mtx_unlock(&ctx->mtx);
			break;
		}
		sync_mtx_unlock(&ctx->mtx);

		for(size_t j = 0; j < ctx->batch_count; ++j)
		{
			if(ctx->batch[j])
			{
				bench_free(ctx->batch[j], ctx->alloc_size);
				ctx->batch[j] = NULL;
			}
		}

		sync_mtx_lock(&ctx->mtx);
		ctx->phase = 0;
		sync_cond_wake(&ctx->cv);
		sync_mtx_unlock(&ctx->mtx);
	}
}


void
bench_handoff_postfree(
	size_t alloc_size
	)
{
	handoff_postfree_ctx_t ctx = {0};
	ctx.alloc_size = alloc_size;
	ctx.iters = HANDOFF_POSTFREE_ITERS;
	ctx.batch_count = HANDOFF_POSTFREE_BATCH;
	ctx.batch = malloc(sizeof(*ctx.batch) * ctx.batch_count);
	memset(ctx.batch, 0, sizeof(*ctx.batch) * ctx.batch_count);
	ctx.samples = malloc(sizeof(*ctx.samples) * ctx.iters);

	sync_mtx_init(&ctx.mtx);
	sync_cond_init(&ctx.cv);

	thread_t prod;
	thread_t cons;
	thread_init(&prod, (thread_data_t){ handoff_postfree_producer, &ctx });
	thread_init(&cons, (thread_data_t){ handoff_postfree_consumer, &ctx });

	thread_join(prod);
	thread_join(cons);

	sync_cond_free(&ctx.cv);
	sync_mtx_free(&ctx.mtx);

	stats_t s = compute_stats(ctx.samples, ctx.iters);
	char label[80];
	snprintf(label, sizeof(label), "post-free next alloc (%5zuB)", alloc_size);
	print_stats(label, &s);

	free(ctx.samples);
	free(ctx.batch);
}


void
bench_section_foreign_free_size(
	size_t alloc_size
	)
{
	bench_emit_desc(
		"handoff",
		"cross-thread producer consumer handoff",
		"producer alloc and consumer free latency",
		"ring handoff with paired producer consumer",
		"remote free path and handoff overhead");

	bench_stat_context = BENCH_STAT_CONTEXT_FOREIGN;
	bench_stat_size = alloc_size;

#ifdef _WIN32
	bench_foreign_free(alloc_size);
#else
	pid_t pid = fork();
	if(!pid)
	{
		bench_foreign_free(alloc_size);
		_exit(0);
	}

	int status;
	waitpid(pid, &status, 0);
#endif

	bench_stat_context = BENCH_STAT_CONTEXT_NONE;
	bench_stat_size = 0;
}


void
bench_section_handoff_postfree_size(
	size_t alloc_size
	)
{
	bench_emit_desc(
		"handoff",
		"cross-thread producer consumer handoff",
		"producer alloc + consumer free + post-free next alloc latency",
		"batched producer handoff then consumer drains all before timed producer alloc",
		"remote free recycling effects on subsequent producer allocation");

	bench_stat_context = BENCH_STAT_CONTEXT_FOREIGN;
	bench_stat_size = alloc_size;

#ifdef _WIN32
	bench_handoff_postfree(alloc_size);
#else
	pid_t pid = fork();
	if(!pid)
	{
		bench_handoff_postfree(alloc_size);
		_exit(0);
	}

	int status;
	waitpid(pid, &status, 0);
#endif

	bench_stat_context = BENCH_STAT_CONTEXT_NONE;
	bench_stat_size = 0;
}


int
main(
	int argc,
	char** argv
	)
{
	bench_common_init(argc, argv);

	if(argc < 4)
	{
		return 1;
	}

	const char* type = argv[2];
	size_t alloc_size = strtoul(argv[3], NULL, 0);

	if(!strcmp(type, "postfree"))
	{
		bench_section_handoff_postfree_size(alloc_size);
	}
	else
	{
		bench_section_foreign_free_size(alloc_size);
	}

	return 0;
}

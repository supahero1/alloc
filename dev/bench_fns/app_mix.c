#include "app_mix.h"

#include <alloc/atomic.h>

#include <stdio.h>
#include <string.h>



size_t
app_mix_pick_size(
	uint32_t* seed
	)
{
	uint32_t r = fast_rand(seed) % 1000;
	int k = 0;

	if(r < 930)
	{
		while(k < 13 && fast_rand(seed) % 100 < 45)
		{
			++k;
		}
	}
	else if(r < 990)
	{
		k = 10 + fast_rand(seed) % 4;
	}
	else
	{
		k = 13;
	}

	size_t base = 8ULL << k;
	if(base > APP_MIX_REGULAR_HIGH_MAX)
	{
		base = APP_MIX_REGULAR_HIGH_MAX;
	}

	if(base <= 512)
	{
		size_t next = base << 1;
		size_t hi = next - 1;
		return base + fast_rand(seed) % (hi - base + 1);
	}

	if(base <= 4096)
	{
		if(fast_rand(seed) % 100 < 70)
		{
			return base;
		}

		size_t next = base << 1;
		size_t hi = next - 1;
		return base + fast_rand(seed) % (hi - base + 1);
	}

	if(fast_rand(seed) % 100 < 94)
	{
		return base;
	}

	size_t step = base >> 4;
	if(!step)
	{
		step = 1;
	}

	size_t delta = fast_rand(seed) % step;
	return base - delta;
}


void
app_mix_touch(
	void* ptr,
	size_t size,
	uint32_t seed
	)
{
	if(!ptr || !size)
	{
		return;
	}

	uint8_t* p = ptr;
	p[0] = seed;
	p[size - 1] = seed >> 8;

	if(size >= 64)
	{
		p[size >> 1] = seed >> 16;
	}
}


void
app_mix_record(
	app_mix_arg_t* a,
	app_mix_kind_t kind,
	uint64_t value
	)
{
	app_mix_reservoir_t* reservoir = &a->reservoirs[kind];
	uint64_t seen = reservoir->seen++;

	if(reservoir->used < APP_MIX_RESERVOIR_SLOTS)
	{
		reservoir->samples[reservoir->used++] = value;
		return;
	}

	uint64_t r = ((uint64_t) fast_rand(&a->sample_seed) << 31) | fast_rand(&a->sample_seed);
	uint64_t idx = r % (seen + 1);

	if(idx < APP_MIX_RESERVOIR_SLOTS)
	{
		reservoir->samples[idx] = value;
	}
}


void
app_mix_free(
	app_mix_arg_t* a,
	void* ptr,
	size_t size,
	int timed,
	int foreign
	)
{
	uint64_t t0 = timed ? get_ns() : 0;
	bench_free(ptr, size);

	if(timed)
	{
		uint64_t elapsed = get_ns() - t0;

		app_mix_record(a, APP_MIX_KIND_FREE, elapsed);

		if(foreign)
		{
			app_mix_record(a, APP_MIX_KIND_FOREIGN_FREE, elapsed);
		}
	}

	++a->free_ops;
}


void*
app_mix_realloc(
	app_mix_arg_t* a,
	void* ptr,
	size_t old_size,
	size_t new_size,
	int timed
	)
{
	uint64_t t0 = timed ? get_ns() : 0;
	void* p = bench_realloc(ptr, old_size, new_size, 0);

	if(timed)
	{
		app_mix_record(a, APP_MIX_KIND_REALLOC, get_ns() - t0);
	}

	++a->realloc_ops;

	return p;
}


void
app_mix_thread(
	void* arg
	)
{
	app_mix_arg_t* a = arg;
	app_mix_ctx_t* ctx = a->ctx;
	uint32_t seed = a->seed;

	struct
	{
		void* ptr;
		size_t size;
	}
	local[APP_MIX_LOCAL_SLOTS];

	for(int i = 0; i < APP_MIX_LOCAL_SLOTS; ++i)
	{
		local[i].ptr = NULL;
		local[i].size = 0;
	}

	uint64_t i = 0;

	while(1)
	{
		if(!(i & APP_MIX_STOP_POLL_MASK))
		{
			if(atomic_load_rx(&ctx->stop) || get_ns() >= ctx->deadline_ns)
			{
				atomic_store_rx(&ctx->stop, 1);
				break;
			}
		}

		uint32_t r = fast_rand(&seed);
		int action = r % 100;
		int local_idx = (r >> 8) % APP_MIX_LOCAL_SLOTS;
		int timed = !((r >> 28) & APP_MIX_TIMED_MASK);

		if(action < 38)
		{
			size_t size = app_mix_pick_size(&seed);

			if(local[local_idx].ptr)
			{
				app_mix_free(a, local[local_idx].ptr, local[local_idx].size, timed, 0);
			}

			uint64_t t0 = timed ? get_ns() : 0;
			void* p = bench_alloc(size, action & 1);

			if(timed)
			{
				app_mix_record(a, APP_MIX_KIND_ALLOC, get_ns() - t0);
			}

			++a->alloc_ops;

			local[local_idx].ptr = p;
			local[local_idx].size = p ? size : 0;
			app_mix_touch(p, size, seed);
		}
		else if(action < 63)
		{
			if(local[local_idx].ptr)
			{
				app_mix_free(a, local[local_idx].ptr, local[local_idx].size, timed, 0);

				local[local_idx].ptr = NULL;
				local[local_idx].size = 0;
			}
		}
		else if(action < 78 || (action < 90 && !a->shared_enabled))
		{
			if(local[local_idx].ptr)
			{
				size_t new_size = app_mix_pick_size(&seed);
				void* p = app_mix_realloc(a, local[local_idx].ptr, local[local_idx].size, new_size, timed);

				if(p)
				{
					local[local_idx].ptr = p;
					local[local_idx].size = new_size;
					app_mix_touch(p, new_size, seed);
				}
			}
		}
		else if(action < 90)
		{
			if(local[local_idx].ptr)
			{
				int shared_idx = fast_rand(&seed) % APP_MIX_SHARED_SLOTS;
				app_mix_slot_t* slot = &ctx->shared[shared_idx];

				sync_mtx_lock(&slot->mutex);

				if(slot->ptr)
				{
					app_mix_free(a, slot->ptr, slot->size, timed, 1);
				}

				slot->ptr = local[local_idx].ptr;
				slot->size = local[local_idx].size;

				local[local_idx].ptr = NULL;
				local[local_idx].size = 0;

				sync_mtx_unlock(&slot->mutex);
				++a->handoffs;
			}
		}
		else if(!a->shared_enabled)
		{
			if(local[local_idx].ptr)
			{
				app_mix_free(a, local[local_idx].ptr, local[local_idx].size, timed, 0);

				local[local_idx].ptr = NULL;
				local[local_idx].size = 0;
			}
		}
		else
		{
			int shared_idx = fast_rand(&seed) % APP_MIX_SHARED_SLOTS;
			app_mix_slot_t* slot = &ctx->shared[shared_idx];

			sync_mtx_lock(&slot->mutex);

			if(slot->ptr)
			{
				if(fast_rand(&seed) & 1)
				{
					if(local[local_idx].ptr)
					{
						app_mix_free(a, local[local_idx].ptr, local[local_idx].size, timed, 0);
					}

					local[local_idx].ptr = slot->ptr;
					local[local_idx].size = slot->size;
				}
				else
				{
					app_mix_free(a, slot->ptr, slot->size, timed, 1);
				}

				slot->ptr = NULL;
				slot->size = 0;
			}

			sync_mtx_unlock(&slot->mutex);
		}

		++i;
	}

	for(int i = 0; i < APP_MIX_LOCAL_SLOTS; ++i)
	{
		if(local[i].ptr)
		{
			app_mix_free(a, local[i].ptr, local[i].size, 0, 0);
		}
	}

	a->executed_ops = i;
}


stats_t
app_mix_merge_stats(
	app_mix_arg_t* args,
	int threads,
	app_mix_kind_t kind
	)
{
	size_t total = 0;
	for(int i = 0; i < threads; ++i)
	{
		total += args[i].reservoirs[kind].used;
	}

	uint64_t* merged = malloc(sizeof(uint64_t) * (total ? total : 1));
	size_t offset = 0;

	for(int i = 0; i < threads; ++i)
	{
		app_mix_reservoir_t* reservoir = &args[i].reservoirs[kind];
		memcpy(merged + offset, reservoir->samples, sizeof(uint64_t) * reservoir->used);
		offset += reservoir->used;
	}

	stats_t s = compute_stats(merged, total);
	free(merged);
	return s;
}


void
bench_app_mix(
	const char* variant,
	const char* stats_prefix,
	int threads,
	uint64_t runtime_ns,
	int shared_enabled,
	uint32_t seed_tag
	)
{
	app_mix_ctx_t ctx = {0};

	for(int i = 0; i < APP_MIX_SHARED_SLOTS; ++i)
	{
		sync_mtx_init(&ctx.shared[i].mutex);
		ctx.shared[i].ptr = NULL;
		ctx.shared[i].size = 0;
	}

	thread_t workers[APP_MIX_THREADS];
	app_mix_arg_t args[APP_MIX_THREADS];

	if(threads < 1)
	{
		threads = 1;
	}

	if(threads > APP_MIX_THREADS)
	{
		threads = APP_MIX_THREADS;
	}

	for(int i = 0; i < threads; ++i)
	{
		args[i] = (app_mix_arg_t){0};
		args[i].ctx = &ctx;
		args[i].shared_enabled = shared_enabled;
		args[i].seed = bench_seed_derive(seed_tag, i);
		args[i].sample_seed = bench_seed_derive(seed_tag ^ 0x5A3F1E00U, i);

		for(int k = 0; k < APP_MIX_KIND__COUNT; ++k)
		{
			args[i].reservoirs[k].samples = malloc(sizeof(uint64_t) * APP_MIX_RESERVOIR_SLOTS);
		}
	}

	uint64_t t0 = get_ns();
	ctx.deadline_ns = t0 + runtime_ns;

	for(int i = 0; i < threads; ++i)
	{
		thread_init(&workers[i], (thread_data_t){ app_mix_thread, &args[i] });
	}

	for(int i = 0; i < threads; ++i)
	{
		thread_join(workers[i]);
	}

	uint64_t t1 = get_ns();
	double runtime_s = (t1 - t0) / 1000000000.0;
	double runtime_ms = (t1 - t0) / 1000000.0;

	for(int i = 0; i < APP_MIX_SHARED_SLOTS; ++i)
	{
		if(ctx.shared[i].ptr)
		{
			bench_free(ctx.shared[i].ptr, ctx.shared[i].size);
		}
		sync_mtx_free(&ctx.shared[i].mutex);
	}

	uint64_t alloc_ops = 0;
	uint64_t free_ops = 0;
	uint64_t realloc_ops = 0;
	uint64_t handoffs = 0;
	uint64_t total_executed = 0;

	for(int i = 0; i < threads; ++i)
	{
		alloc_ops += args[i].alloc_ops;
		free_ops += args[i].free_ops;
		realloc_ops += args[i].realloc_ops;
		handoffs += args[i].handoffs;
		total_executed += args[i].executed_ops;
	}

	stats_t sa = app_mix_merge_stats(args, threads, APP_MIX_KIND_ALLOC);
	stats_t sf = app_mix_merge_stats(args, threads, APP_MIX_KIND_FREE);
	stats_t sr = app_mix_merge_stats(args, threads, APP_MIX_KIND_REALLOC);
	stats_t sff = app_mix_merge_stats(args, threads, APP_MIX_KIND_FOREIGN_FREE);

	for(int i = 0; i < threads; ++i)
	{
		for(int k = 0; k < APP_MIX_KIND__COUNT; ++k)
		{
			free(args[i].reservoirs[k].samples);
		}
	}

	uint64_t total_ops = alloc_ops + free_ops + realloc_ops;
	double throughput = runtime_s ? total_ops / runtime_s / 1000000.0 : 0;
	uint64_t ops_per_thread = threads ? total_executed / threads : 0;

	if(shared_enabled)
	{
		bench_log("BENCH|type=app_mix|variant=", variant,
			"|threads=", threads,
			"|ops_per_thread=", ops_per_thread,
			"|total_ops=", total_ops,
			"|handoffs=", handoffs,
			"|runtime=", runtime_ms,
			"|throughput=", throughput);
	}
	else
	{
		bench_log("BENCH|type=app_mix|variant=", variant,
			"|threads=", threads,
			"|ops_per_thread=", ops_per_thread,
			"|total_ops=", total_ops,
			"|runtime=", runtime_ms,
			"|throughput=", throughput);
	}

	char label[64];
	snprintf(label, sizeof(label), "%s alloc", stats_prefix);
	print_stats(label, &sa);
	snprintf(label, sizeof(label), "%s free", stats_prefix);
	print_stats(label, &sf);
	snprintf(label, sizeof(label), "%s realloc", stats_prefix);
	print_stats(label, &sr);

	if(shared_enabled)
	{
		snprintf(label, sizeof(label), "%s foreign_free", stats_prefix);
		print_stats(label, &sff);
	}
}

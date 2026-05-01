#include "app_mix.h"

#include <alloc/atomic.h>

#include <stdio.h>



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
	uint32_t _Atomic* count,
	uint64_t* samples,
	uint64_t value
	)
{
	uint32_t idx = atomic_fetch_add_rx(count, 1);

	if(idx < APP_MIX_MAX_SAMPLES)
	{
		samples[idx] = value;
	}
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

		if(action < 38)
		{
			size_t size = app_mix_pick_size(&seed);

			if(local[local_idx].ptr)
			{
				uint64_t tf0 = get_ns();
				bench_free(local[local_idx].ptr, local[local_idx].size);
				uint64_t tf1 = get_ns();
				app_mix_record(&ctx->free_n, ctx->free_samples, tf1 - tf0);
				atomic_fetch_add_rx(&ctx->free_ops, 1);
			}

			uint64_t t0 = get_ns();
			void* p = bench_alloc(size, action & 1);
			uint64_t t1 = get_ns();

			app_mix_record(&ctx->alloc_n, ctx->alloc_samples, t1 - t0);
			atomic_fetch_add_rx(&ctx->alloc_ops, 1);

			local[local_idx].ptr = p;
			local[local_idx].size = p ? size : 0;
			app_mix_touch(p, size, seed);
		}
		else if(action < 63)
		{
			if(local[local_idx].ptr)
			{
				uint64_t t0 = get_ns();
				bench_free(local[local_idx].ptr, local[local_idx].size);
				uint64_t t1 = get_ns();

				app_mix_record(&ctx->free_n, ctx->free_samples, t1 - t0);
				atomic_fetch_add_rx(&ctx->free_ops, 1);

				local[local_idx].ptr = NULL;
				local[local_idx].size = 0;
			}
		}
		else if(action < 78)
		{
			if(local[local_idx].ptr)
			{
				size_t new_size = app_mix_pick_size(&seed);

				uint64_t t0 = get_ns();
				void* p = bench_realloc(local[local_idx].ptr, local[local_idx].size, new_size, 0);
				uint64_t t1 = get_ns();

				app_mix_record(&ctx->realloc_n, ctx->realloc_samples, t1 - t0);
				atomic_fetch_add_rx(&ctx->realloc_ops, 1);

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
			if(!a->shared_enabled)
			{
				if(local[local_idx].ptr)
				{
					size_t new_size = app_mix_pick_size(&seed);

					uint64_t t0 = get_ns();
					void* p = bench_realloc(local[local_idx].ptr, local[local_idx].size, new_size, 0);
					uint64_t t1 = get_ns();

					app_mix_record(&ctx->realloc_n, ctx->realloc_samples, t1 - t0);
					atomic_fetch_add_rx(&ctx->realloc_ops, 1);

					if(p)
					{
						local[local_idx].ptr = p;
						local[local_idx].size = new_size;
						app_mix_touch(p, new_size, seed);
					}
				}

				continue;
			}

			if(local[local_idx].ptr)
			{
				int shared_idx = fast_rand(&seed) % APP_MIX_SHARED_SLOTS;
				app_mix_slot_t* slot = &ctx->shared[shared_idx];

				sync_mtx_lock(&slot->mutex);

				if(slot->ptr)
				{
					uint64_t t0 = get_ns();
					bench_free(slot->ptr, slot->size);
					uint64_t t1 = get_ns();

					app_mix_record(&ctx->foreign_free_n, ctx->foreign_free_samples, t1 - t0);
					app_mix_record(&ctx->free_n, ctx->free_samples, t1 - t0);
					atomic_fetch_add_rx(&ctx->free_ops, 1);
				}

				slot->ptr = local[local_idx].ptr;
				slot->size = local[local_idx].size;

				local[local_idx].ptr = NULL;
				local[local_idx].size = 0;

				sync_mtx_unlock(&slot->mutex);
				atomic_fetch_add_rx(&ctx->foreign_handoffs, 1);
			}
		}
		else
		{
			if(!a->shared_enabled)
			{
				if(local[local_idx].ptr)
				{
					uint64_t t0 = get_ns();
					bench_free(local[local_idx].ptr, local[local_idx].size);
					uint64_t t1 = get_ns();

					app_mix_record(&ctx->free_n, ctx->free_samples, t1 - t0);
					atomic_fetch_add_rx(&ctx->free_ops, 1);

					local[local_idx].ptr = NULL;
					local[local_idx].size = 0;
				}

				continue;
			}

			int shared_idx = fast_rand(&seed) % APP_MIX_SHARED_SLOTS;
			app_mix_slot_t* slot = &ctx->shared[shared_idx];

			sync_mtx_lock(&slot->mutex);

			if(slot->ptr)
			{
				if(fast_rand(&seed) & 1)
				{
					if(local[local_idx].ptr)
					{
						uint64_t tf0 = get_ns();
						bench_free(local[local_idx].ptr, local[local_idx].size);
						uint64_t tf1 = get_ns();

						app_mix_record(&ctx->free_n, ctx->free_samples, tf1 - tf0);
						atomic_fetch_add_rx(&ctx->free_ops, 1);
					}

					local[local_idx].ptr = slot->ptr;
					local[local_idx].size = slot->size;
					slot->ptr = NULL;
					slot->size = 0;
				}
				else
				{
					uint64_t t0 = get_ns();
					bench_free(slot->ptr, slot->size);
					uint64_t t1 = get_ns();

					app_mix_record(&ctx->foreign_free_n, ctx->foreign_free_samples, t1 - t0);
					app_mix_record(&ctx->free_n, ctx->free_samples, t1 - t0);
					atomic_fetch_add_rx(&ctx->free_ops, 1);

					slot->ptr = NULL;
					slot->size = 0;
				}
			}

			sync_mtx_unlock(&slot->mutex);
		}

		++i;
	}

	for(int i = 0; i < APP_MIX_LOCAL_SLOTS; ++i)
	{
		if(local[i].ptr)
		{
			uint64_t t0 = get_ns();
			bench_free(local[i].ptr, local[i].size);
			uint64_t t1 = get_ns();

			app_mix_record(&ctx->free_n, ctx->free_samples, t1 - t0);
			atomic_fetch_add_rx(&ctx->free_ops, 1);
		}
	}

	a->executed_ops = i;

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

	ctx.alloc_samples = malloc(sizeof(uint64_t) * APP_MIX_MAX_SAMPLES);
	ctx.free_samples = malloc(sizeof(uint64_t) * APP_MIX_MAX_SAMPLES);
	ctx.realloc_samples = malloc(sizeof(uint64_t) * APP_MIX_MAX_SAMPLES);
	ctx.foreign_free_samples = malloc(sizeof(uint64_t) * APP_MIX_MAX_SAMPLES);

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

	uint64_t t0 = get_ns();
	ctx.deadline_ns = t0 + runtime_ns;

	for(int i = 0; i < threads; ++i)
	{
		args[i].ctx = &ctx;
		args[i].shared_enabled = shared_enabled;
		args[i].seed = bench_seed_derive(seed_tag, i);
		args[i].executed_ops = 0;
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

	size_t alloc_n = atomic_load_rx(&ctx.alloc_n);
	size_t free_n = atomic_load_rx(&ctx.free_n);
	size_t realloc_n = atomic_load_rx(&ctx.realloc_n);
	size_t foreign_free_n = atomic_load_rx(&ctx.foreign_free_n);

	alloc_n = MACRO_MIN(alloc_n, APP_MIX_MAX_SAMPLES);
	free_n = MACRO_MIN(free_n, APP_MIX_MAX_SAMPLES);
	realloc_n = MACRO_MIN(realloc_n, APP_MIX_MAX_SAMPLES);
	foreign_free_n = MACRO_MIN(foreign_free_n, APP_MIX_MAX_SAMPLES);

	stats_t sa = compute_stats(ctx.alloc_samples, alloc_n);
	stats_t sf = compute_stats(ctx.free_samples, free_n);
	stats_t sr = compute_stats(ctx.realloc_samples, realloc_n);
	stats_t sff = compute_stats(ctx.foreign_free_samples, foreign_free_n);

	uint64_t alloc_ops = atomic_load_rx(&ctx.alloc_ops);
	uint64_t free_ops = atomic_load_rx(&ctx.free_ops);
	uint64_t realloc_ops = atomic_load_rx(&ctx.realloc_ops);
	uint64_t handoffs = atomic_load_rx(&ctx.foreign_handoffs);
	uint64_t total_ops = alloc_ops + free_ops + realloc_ops;
	double throughput = runtime_s ? total_ops / runtime_s / 1000000.0 : 0;

	uint64_t total_executed = 0;
	for(int i = 0; i < threads; ++i)
	{
		total_executed += args[i].executed_ops;
	}
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

	free(ctx.alloc_samples);
	free(ctx.free_samples);
	free(ctx.realloc_samples);
	free(ctx.foreign_free_samples);
}

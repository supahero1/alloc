#include "common.h"

#include <string.h>
#include <sys/wait.h>

#define BENCH_ITERS 500000
#define BENCH_MEM_CAP (256 * 1024 * 1024)


int
bench_iters(
	size_t alloc_size
	)
{
	size_t iters = BENCH_MEM_CAP / alloc_size;
	iters = MACRO_CLAMP(iters, 1000, BENCH_ITERS);
	return iters;
}


stats_t
bench_malloc(
	size_t alloc_size
	)
{
	int n = bench_iters(alloc_size);
	uint64_t* samples = malloc(sizeof(uint64_t) * n);
	void** ptrs = malloc(sizeof(void*) * n);

	for(int i = 0; i < n; ++i)
	{
		uint64_t t0 = get_ns();
		ptrs[i] = bench_alloc(alloc_size, 0);
		uint64_t t1 = get_ns();
		samples[i] = t1 - t0;
	}

	for(int i = 0; i < n; ++i)
	{
		bench_free(ptrs[i], alloc_size);
	}

	stats_t s = compute_stats(samples, n);
	free(ptrs);
	free(samples);
	return s;
}


stats_t
bench_calloc(
	size_t alloc_size
	)
{
	int n = bench_iters(alloc_size);
	uint64_t* samples = malloc(sizeof(uint64_t) * n);
	void** ptrs = malloc(sizeof(void*) * n);

	for(int i = 0; i < n; ++i)
	{
		uint64_t t0 = get_ns();
		ptrs[i] = bench_alloc(alloc_size, 1);
		uint64_t t1 = get_ns();
		samples[i] = t1 - t0;
	}

	for(int i = 0; i < n; ++i)
	{
		bench_free(ptrs[i], alloc_size);
	}

	stats_t s = compute_stats(samples, n);
	free(ptrs);
	free(samples);
	return s;
}


stats_t
bench_aligned_malloc(
	size_t alloc_size
	)
{
	int n = bench_iters(alloc_size);
	uint64_t* samples = malloc(sizeof(uint64_t) * n);
	void** ptrs = malloc(sizeof(void*) * n);

	for(int i = 0; i < n; ++i)
	{
		uint64_t t0 = get_ns();
		ptrs[i] = bench_alloc_aligned(alloc_size, 64, 0);
		uint64_t t1 = get_ns();
		samples[i] = t1 - t0;
	}

	for(int i = 0; i < n; ++i)
	{
		bench_free(ptrs[i], alloc_size);
	}

	stats_t s = compute_stats(samples, n);
	free(ptrs);
	free(samples);
	return s;
}


stats_t
bench_aligned_free(
	size_t alloc_size
	)
{
	int n = bench_iters(alloc_size);
	uint64_t* samples = malloc(sizeof(uint64_t) * n);
	void** ptrs = malloc(sizeof(void*) * n);

	for(int i = 0; i < n; ++i)
	{
		ptrs[i] = bench_alloc_aligned(alloc_size, 64, 0);
	}

	for(int i = 0; i < n; ++i)
	{
		uint64_t t0 = get_ns();
		bench_free(ptrs[i], alloc_size);
		uint64_t t1 = get_ns();
		samples[i] = t1 - t0;
	}

	stats_t s = compute_stats(samples, n);
	free(ptrs);
	free(samples);
	return s;
}


stats_t
bench_free_ops(
	size_t alloc_size
	)
{
	int n = bench_iters(alloc_size);
	uint64_t* samples = malloc(sizeof(uint64_t) * n);
	void** ptrs = malloc(sizeof(void*) * n);

	for(int i = 0; i < n; ++i)
	{
		ptrs[i] = bench_alloc(alloc_size, 0);
	}

	for(int i = 0; i < n; ++i)
	{
		uint64_t t0 = get_ns();
		bench_free(ptrs[i], alloc_size);
		uint64_t t1 = get_ns();
		samples[i] = t1 - t0;
	}

	stats_t s = compute_stats(samples, n);
	free(ptrs);
	free(samples);
	return s;
}


stats_t
bench_realloc_small(
	size_t base_size
	)
{
	int n = bench_iters(base_size);
	uint64_t* samples = malloc(sizeof(uint64_t) * n);

	void* ptr = bench_alloc(base_size, 0);
	size_t cur_size = base_size;
	uint32_t seed = bench_seed_derive(0x5A11A0C1U, base_size);

	for(int i = 0; i < n; ++i)
	{
		int delta = fast_rand(&seed) % 9 - 4;
		size_t new_size = cur_size;

		if(delta >= 0)
		{
			new_size = cur_size + delta;
		}
		else
		{
			size_t drop = -delta;
			if(drop < cur_size)
			{
				new_size = cur_size - drop;
			}
		}

		if(new_size < 2)
		{
			new_size = 2;
		}

		uint64_t t0 = get_ns();
		void* new_ptr = bench_realloc(ptr, cur_size, new_size, 0);
		uint64_t t1 = get_ns();

		if(new_ptr)
		{
			ptr = new_ptr;
			cur_size = new_size;
		}

		samples[i] = t1 - t0;
	}

	bench_free(ptr, cur_size);

	stats_t s = compute_stats(samples, n);
	free(samples);
	return s;
}


stats_t
bench_realloc_grow_end(
	size_t base_size
	)
{
	int n = bench_iters(base_size) / 2;
	uint64_t* samples = malloc(sizeof(uint64_t) * n);

	for(int i = 0; i < n; ++i)
	{
		void* ptr = bench_alloc(base_size, 0);
		size_t big = base_size * 3;

		uint64_t t0 = get_ns();
		void* new_ptr = bench_realloc(ptr, base_size, big, 0);
		uint64_t t1 = get_ns();

		bench_free(new_ptr ? new_ptr : ptr, new_ptr ? big : base_size);
		samples[i] = t1 - t0;
	}

	stats_t s = compute_stats(samples, n);
	free(samples);
	return s;
}


stats_t
bench_realloc_grow_middle(
	size_t base_size
	)
{
	int n = bench_iters(base_size) / 2;
	uint64_t* samples = malloc(sizeof(uint64_t) * n);

	for(int i = 0; i < n; ++i)
	{
		void* target = bench_alloc(base_size, 0);
		void* blocker = bench_alloc(base_size, 0);
		size_t big = base_size * 3;

		uint64_t t0 = get_ns();
		void* new_ptr = bench_realloc(target, base_size, big, 0);
		uint64_t t1 = get_ns();

		bench_free(new_ptr ? new_ptr : target, new_ptr ? big : base_size);
		bench_free(blocker, base_size);
		samples[i] = t1 - t0;
	}

	stats_t s = compute_stats(samples, n);
	free(samples);
	return s;
}


stats_t
bench_realloc_shrink(
	size_t base_size
	)
{
	int n = bench_iters(base_size) / 2;
	uint64_t* samples = malloc(sizeof(uint64_t) * n);

	for(int i = 0; i < n; ++i)
	{
		size_t big = base_size * 3;
		void* ptr = bench_alloc(big, 0);
		size_t small = base_size;

		uint64_t t0 = get_ns();
		void* new_ptr = bench_realloc(ptr, big, small, 0);
		uint64_t t1 = get_ns();

		bench_free(new_ptr ? new_ptr : ptr, new_ptr ? small : big);
		samples[i] = t1 - t0;
	}

	stats_t s = compute_stats(samples, n);
	free(samples);
	return s;
}


stats_t
bench_realloc_class_jump(
	size_t base_size
	)
{
	int n = bench_iters(base_size) / 2;
	uint64_t* samples = malloc(sizeof(uint64_t) * n);

	for(int i = 0; i < n; ++i)
	{
		void* target = bench_alloc(base_size, 0);
		void* blocker = bench_alloc(base_size, 0);
		size_t big = base_size * 8;

		uint64_t t0 = get_ns();
		void* new_ptr = bench_realloc(target, base_size, big, 0);
		uint64_t t1 = get_ns();

		bench_free(new_ptr ? new_ptr : target, new_ptr ? big : base_size);
		bench_free(blocker, base_size);
		samples[i] = t1 - t0;
	}

	stats_t s = compute_stats(samples, n);
	free(samples);
	return s;
}


stats_t
bench_malloc_hot(
	size_t alloc_size
	)
{
	uint64_t* samples = malloc(sizeof(uint64_t) * BENCH_ITERS);

	for(int i = 0; i < BENCH_ITERS; ++i)
	{
		uint64_t t0 = get_ns();
		void* p = bench_alloc(alloc_size, 0);
		uint64_t t1 = get_ns();
		bench_free(p, alloc_size);
		samples[i] = t1 - t0;
	}

	stats_t s = compute_stats(samples, BENCH_ITERS);
	free(samples);
	return s;
}


stats_t
bench_calloc_hot(
	size_t alloc_size
	)
{
	uint64_t* samples = malloc(sizeof(uint64_t) * BENCH_ITERS);

	for(int i = 0; i < BENCH_ITERS; ++i)
	{
		uint64_t t0 = get_ns();
		void* p = bench_alloc(alloc_size, 1);
		uint64_t t1 = get_ns();
		bench_free(p, alloc_size);
		samples[i] = t1 - t0;
	}

	stats_t s = compute_stats(samples, BENCH_ITERS);
	free(samples);
	return s;
}


stats_t
bench_free_hot(
	size_t alloc_size
	)
{
	uint64_t* samples = malloc(sizeof(uint64_t) * BENCH_ITERS);

	for(int i = 0; i < BENCH_ITERS; ++i)
	{
		void* p = bench_alloc(alloc_size, 0);
		uint64_t t0 = get_ns();
		bench_free(p, alloc_size);
		uint64_t t1 = get_ns();
		samples[i] = t1 - t0;
	}

	stats_t s = compute_stats(samples, BENCH_ITERS);
	free(samples);
	return s;
}


stats_t
bench_malloc_free_pair(
	size_t alloc_size
	)
{
	uint64_t* samples = malloc(sizeof(uint64_t) * BENCH_ITERS);

	for(int i = 0; i < BENCH_ITERS; ++i)
	{
		uint64_t t0 = get_ns();
		void* p = bench_alloc(alloc_size, 0);
		bench_free(p, alloc_size);
		uint64_t t1 = get_ns();
		samples[i] = t1 - t0;
	}

	stats_t s = compute_stats(samples, BENCH_ITERS);
	free(samples);
	return s;
}


void
bench_run_stats_case_for_size(
	size_t alloc_size,
	const char* label,
	stats_t (*fn)(size_t)
	)
{
	bench_stat_size = alloc_size;
	pid_t pid = fork();
	if(!pid)
	{
		stats_t s = fn(alloc_size);
		print_stats(label, &s);
		_exit(0);
	}

	int status;
	waitpid(pid, &status, 0);
}


stats_t
bench_realloc_small_for_ops(
	size_t alloc_size
	)
{
	size_t base = alloc_size > 8 ? alloc_size : 16;
	return bench_realloc_small(base);
}


stats_t
bench_realloc_shrink_for_ops(
	size_t alloc_size
	)
{
	size_t base = alloc_size > 8 ? alloc_size : 16;
	return bench_realloc_shrink(base);
}


stats_t
bench_realloc_class_jump_for_ops(
	size_t alloc_size
	)
{
	size_t base = alloc_size > 8 ? alloc_size : 16;
	return bench_realloc_class_jump(base);
}


void
bench_section_ops_case(
	const char* desc_test,
	const char* desc_what,
	const char* label,
	stats_t (*fn)(size_t),
	const size_t* sizes,
	size_t n_sizes
	)
{
	bench_emit_desc(desc_test,
		desc_what,
		"mean p95 p99 p99.9 latency",
		"isolated forked microbench path by size",
		"fast-path and relocation-path regressions");

	bench_stat_context = BENCH_STAT_CONTEXT_OPS;

	for(size_t i = 0; i < n_sizes; ++i)
	{
		bench_run_stats_case_for_size(sizes[i], label, fn);
	}

	bench_stat_context = BENCH_STAT_CONTEXT_NONE;
	bench_stat_size = 0;
}


int
bench_run_ops_token(
	const char* kind,
	size_t size
	)
{
	size_t one_size[] = { size };

	if(!strcmp(kind, "malloc"))
	{
		bench_section_ops_case(
			"ops / malloc",
			"malloc latency across small and large sizes",
			"malloc",
			bench_malloc,
			one_size,
			1
			);
		return true;
	}

	if(!strcmp(kind, "calloc"))
	{
		bench_section_ops_case(
			"ops / calloc",
			"calloc latency across small sizes",
			"calloc",
			bench_calloc,
			one_size,
			1
			);
		return true;
	}

	if(!strcmp(kind, "aligned_malloc"))
	{
		bench_section_ops_case(
			"ops / aligned malloc",
			"aligned allocation latency across small sizes",
			"aligned malloc",
			bench_aligned_malloc,
			one_size,
			1
			);
		return true;
	}

	if(!strcmp(kind, "aligned_free"))
	{
		bench_section_ops_case(
			"ops / aligned free",
			"aligned free latency across small sizes",
			"aligned free",
			bench_aligned_free,
			one_size,
			1
			);
		return true;
	}

	if(!strcmp(kind, "free"))
	{
		bench_section_ops_case(
			"ops / free",
			"free latency across small and large sizes",
			"free",
			bench_free_ops,
			one_size,
			1
			);
		return true;
	}

	if(!strcmp(kind, "malloc_hot"))
	{
		bench_section_ops_case(
			"ops / malloc latency (reuse cycle)",
			"hot malloc latency across small and large sizes",
			"malloc latency (reuse cycle)",
			bench_malloc_hot,
			one_size,
			1
			);
		return true;
	}

	if(!strcmp(kind, "calloc_hot"))
	{
		bench_section_ops_case(
			"ops / calloc latency (reuse cycle)",
			"hot calloc latency across small sizes",
			"calloc latency (reuse cycle)",
			bench_calloc_hot,
			one_size,
			1
			);
		return true;
	}

	if(!strcmp(kind, "free_hot"))
	{
		bench_section_ops_case(
			"ops / free latency (fresh alloc each iter)",
			"hot free latency across small and large sizes",
			"free latency (fresh alloc each iter)",
			bench_free_hot,
			one_size,
			1
			);
		return true;
	}

	if(!strcmp(kind, "pair"))
	{
		bench_section_ops_case(
			"ops / malloc+free cycle",
			"malloc+free pair latency across small sizes",
			"malloc+free cycle",
			bench_malloc_free_pair,
			one_size,
			1
			);
		return true;
	}

	if(!strcmp(kind, "realloc_small"))
	{
		bench_section_ops_case(
			"ops / realloc small (+-4B)",
			"small-step realloc latency across small sizes",
			"realloc small (+-4B)",
			bench_realloc_small_for_ops,
			one_size,
			1
			);
		return true;
	}

	if(!strcmp(kind, "realloc_grow_end"))
	{
		bench_section_ops_case(
			"ops / realloc grow (3x, no neighbor blocker)",
			"realloc grow latency without neighbor blocker across small sizes",
			"realloc grow (3x, no neighbor blocker)",
			bench_realloc_grow_end,
			one_size,
			1
			);
		return true;
	}

	if(!strcmp(kind, "realloc_grow_middle"))
	{
		bench_section_ops_case(
			"ops / realloc grow (3x, with neighbor blocker)",
			"realloc grow latency with neighbor blocker across small sizes",
			"realloc grow (3x, with neighbor blocker)",
			bench_realloc_grow_middle,
			one_size,
			1
			);
		return true;
	}

	if(!strcmp(kind, "realloc_shrink"))
	{
		bench_section_ops_case(
			"ops / realloc shrink (1/3)",
			"realloc shrink latency across small sizes",
			"realloc shrink (1/3)",
			bench_realloc_shrink_for_ops,
			one_size,
			1
			);
		return true;
	}

	if(!strcmp(kind, "realloc_class_jump"))
	{
		bench_section_ops_case(
			"ops / realloc class jump (x8, blocked)",
			"realloc class-jump latency with blocker across small sizes",
			"realloc class jump (x8, blocked)",
			bench_realloc_class_jump_for_ops,
			one_size,
			1
			);
		return true;
	}

	return false;
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

	const char* kind = argv[2];
	size_t alloc_size = strtoul(argv[3], NULL, 0);

	return bench_run_ops_token(kind, alloc_size) ? 0 : 1;
}

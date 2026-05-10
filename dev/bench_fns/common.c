#include "common.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
	#include <malloc.h>
	#include <windows.h>
	#include <psapi.h>
#endif

#include <alloc/platform.h>

#ifdef DEV_ALLOC
	#include <alloc/base.h>
#elif defined(BENCH_JEMALLOC)
	#include <jemalloc/jemalloc.h>
#elif defined(BENCH_MIMALLOC)
	#include <mimalloc.h>
#elif defined(BENCH_TBBMALLOC)
	#include <oneapi/tbb/scalable_allocator.h>
#endif


bench_stat_context_t bench_stat_context;
size_t bench_stat_size;
uint64_t bench_seed_value;
uint32_t bench_seed_base;


#ifdef DEV_ALLOC


	void*
	bench_alloc(
		size_t size,
		int zero
		)
	{
		return alloc_alloc(NULL, size, zero);
	}


	void
	bench_free(
		const volatile void* ptr,
		size_t size
		)
	{
		alloc_free(ptr, size);
	}

	
	void
	bench_free_aligned(
		const volatile void* ptr,
		size_t size,
		size_t alignment
		)
	{
		alloc_free_aligned_e(ptr, size, alignment);
	}


	void*
	bench_realloc(
		const volatile void* ptr,
		size_t old_size,
		size_t new_size,
		int zero
		)
	{
		return alloc_realloc(ptr, old_size, new_size, zero);
	}


	void*
	bench_alloc_aligned(
		size_t size,
		size_t alignment,
		int zero
		)
	{
		return alloc_alloc_aligned_e(size, alignment, zero);
	}


#else


	void*
	bench_alloc(
		size_t size,
		int zero
		)
	{
#if defined(BENCH_JEMALLOC)
		if(!zero)
		{
			return je_malloc(size);
		}

		return je_calloc(1, size);
#elif defined(BENCH_MIMALLOC)
		if(!zero)
		{
			return mi_malloc(size);
		}

		return mi_calloc(1, size);
#elif defined(BENCH_TBBMALLOC)
		if(!zero)
		{
			return scalable_malloc(size);
		}

		return scalable_calloc(1, size);
#else
		if(!zero)
		{
			return malloc(size);
		}

		return calloc(1, size);
#endif
	}


	void
	bench_free(
		const volatile void* ptr,
		size_t size
		)
	{
		(void) size;

#if defined(BENCH_JEMALLOC)
		je_free((void*) ptr);
#elif defined(BENCH_MIMALLOC)
		mi_free((void*) ptr);
#elif defined(BENCH_TBBMALLOC)
		scalable_free((void*) ptr);
#else
		free((void*) ptr);
#endif
	}

	void
	bench_free_aligned(
		const volatile void* ptr,
		size_t size,
		size_t alignment
		)
	{
		(void) size;

#if defined(BENCH_JEMALLOC)
		(void) alignment;
		je_free((void*) ptr);
#elif defined(BENCH_MIMALLOC)
		mi_free_aligned((void*) ptr, alignment);
#elif defined(BENCH_TBBMALLOC)
		(void) alignment;
		scalable_aligned_free((void*) ptr);
#elif defined(_WIN32)
		(void) alignment;
		_aligned_free((void*) ptr);
#else
		(void) alignment;
		free((void*) ptr);
#endif
	}


	void*
	bench_realloc(
		const volatile void* ptr,
		size_t old_size,
		size_t new_size,
		int zero
		)
	{
#if defined(BENCH_JEMALLOC)
		void* new_ptr = je_realloc((void*) ptr, new_size);
#elif defined(BENCH_MIMALLOC)
		void* new_ptr = mi_realloc((void*) ptr, new_size);
#elif defined(BENCH_TBBMALLOC)
		void* new_ptr = scalable_realloc((void*) ptr, new_size);
#else
		void* new_ptr = realloc((void*) ptr, new_size);
#endif
		if(!new_ptr)
		{
			return NULL;
		}

		if(new_size > old_size && zero)
		{
			memset(new_ptr + old_size, 0, new_size - old_size);
		}

		return new_ptr;
	}


	void*
	bench_alloc_aligned(
		size_t size,
		size_t alignment,
		int zero
		)
	{
#if defined(BENCH_JEMALLOC)
		void* ptr = je_aligned_alloc(alignment, MACRO_ALIGN_UP(size, alignment - 1));
#elif defined(BENCH_MIMALLOC)
		void* ptr = mi_malloc_aligned(size, alignment);
#elif defined(BENCH_TBBMALLOC)
		void* ptr = scalable_aligned_malloc(size, alignment);
#elif defined(_WIN32)
		void* ptr = _aligned_malloc(size, alignment);
		if(!ptr)
		{
			return NULL;
		}
#else
		void* ptr = NULL;
		if(posix_memalign(&ptr, alignment, size))
		{
			return NULL;
		}
#endif

		if(zero)
		{
			memset(ptr, 0, size);
		}

		return ptr;
	}


#endif


uint64_t
get_ns(
	void
	)
{
	return alloc_read_time_ns();
}


uint32_t
fast_rand(
	uint32_t* seed
	)
{
	return *seed = (1103515245 * *seed + 12345) & 0x7FFFFFFF;
}


uint32_t
mix_u32(
	uint32_t x
	)
{
	x ^= x >> 16;
	x *= 0x7feb352dU;
	x ^= x >> 15;
	x *= 0x846ca68bU;
	x ^= x >> 16;

	if(!x)
	{
		x = 1;
	}

	return x;
}


uint32_t
bench_seed_derive(
	uint32_t tag,
	uint32_t extra
	)
{
	uint32_t x = bench_seed_base ^ tag;
	x += 0x9e3779b9U * (extra + 1);
	return mix_u32(x);
}


int
cmp_u64(
	const uint64_t* a,
	const uint64_t* b
	)
{
	return (*a > *b) - (*a < *b);
}


stats_t
compute_stats(
	uint64_t* samples,
	size_t n
	)
{
	stats_t s = {0};
	if(!n)
	{
		return s;
	}

	qsort(samples, n, sizeof(uint64_t), (void*) cmp_u64);

	s.min = samples[0];
	s.max = samples[n - 1];

	double sum = 0;
	for(size_t i = 0; i < n; ++i)
	{
		sum += samples[i];
	}
	s.mean = sum / n;

	if(n >= 2)
	{
		size_t mid = n / 2;
		s.median = n & 1 ? samples[mid] : (samples[mid - 1] + samples[mid]) / 2.0;
	}
	else
	{
		s.median = samples[0];
	}

	double var = 0;
	for(size_t i = 0; i < n; ++i)
	{
		double d = samples[i] - s.mean;
		var += d * d;
	}
	s.stddev = sqrt(var / n);

	s.p50 = samples[(n - 1) / 2];
	s.p90 = samples[(n - 1) * 9 / 10];
	s.p95 = samples[(n - 1) * 95 / 100];
	s.p99 = samples[(n - 1) * 99 / 100];
	s.p999 = samples[(n - 1) * 999 / 1000];

	return s;
}


const char*
bench_stat_context_name(
	void
	)
{
	switch(bench_stat_context)
	{

	case BENCH_STAT_CONTEXT_OPS: return "ops";
	case BENCH_STAT_CONTEXT_FIRST: return "first";
	case BENCH_STAT_CONTEXT_FOREIGN: return "foreign";
	case BENCH_STAT_CONTEXT_APP_MT: return "app_mt";
	case BENCH_STAT_CONTEXT_APP_ST: return "app_st";
	case BENCH_STAT_CONTEXT_LOCALITY: return "locality";
	default: return "none";

	}
}


void
bench_emit_stat_row(
	const char* label,
	const stats_t* s
	)
{
	if(!label || !s)
	{
		return;
	}

	bench_log("BENCH|type=stat|ctx=", bench_stat_context_name(),
		"|size=", bench_stat_size,
		"|label=", label,
		"|mean=", s->mean,
		"|stddev=", s->stddev,
		"|p95=", s->p95,
		"|p99=", s->p99,
		"|p99.9=", s->p999);
}


void
print_stats(
	const char* label,
	stats_t* s
	)
{
	bench_log("  ", label,
		" min=", s->min,
		" max=", s->max,
		" mean=", s->mean,
		" med=", s->median,
		" std=", s->stddev,
		" p50=", s->p50,
		" p90=", s->p90,
		" p95=", s->p95,
		" p99=", s->p99,
		" p99.9=", s->p999,
		" ns");

	bench_emit_stat_row(label, s);
}


long
get_rss_kb(
	void
	)
{
#ifdef _WIN32
	PROCESS_MEMORY_COUNTERS_EX pmc;
	if(!GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*) &pmc, sizeof(pmc)))
	{
		return -1;
	}

	return (long) (pmc.WorkingSetSize / 1024);
#else
	FILE* f = fopen("/proc/self/statm", "r");
	if(!f)
	{
		return -1;
	}

	long virt, rss;
	if(fscanf(f, "%ld %ld", &virt, &rss) != 2)
	{
		fclose(f);
		return -1;
	}

	fclose(f);
	return rss * 4;
#endif
}


long
get_anon_hugepages_kb(
	void
	)
{
#ifdef _WIN32
	return 0;
#else
	FILE* f = fopen("/proc/self/smaps_rollup", "r");
	if(!f)
	{
		return 0;
	}

	char line[256];
	long val = 0;

	while(fgets(line, sizeof(line), f) && sscanf(line, "AnonHugePages: %ld kB", &val) != 1);

	fclose(f);
	return val;
#endif
}


void
bench_emit_desc(
	const char* test,
	const char* what,
	const char* measures,
	const char* method,
	const char* detects
	)
{
	if(!test || !what || !measures || !method || !detects)
	{
		return;
	}

	bench_log("BENCH|type=desc|test=", test,
		"|what=", what,
		"|measures=", measures,
		"|method=", method,
		"|detects=", detects);
}


void
bench_common_init(
	int argc,
	char** argv
	)
{
	if(argc >= 2)
	{
		bench_seed_value = strtoull(argv[1], NULL, 0);
	}
	else
	{
		bench_seed_value = get_ns();
	}

	bench_seed_base = mix_u32(bench_seed_value ^ bench_seed_value >> 32);

	bench_log("BENCH|type=meta|seed=", bench_seed_value);
}

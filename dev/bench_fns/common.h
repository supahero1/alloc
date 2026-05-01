#pragma once

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <alloc/log.h>
#include <alloc/macro.h>
#include <alloc/sync.h>
#include <alloc/threads.h>


typedef enum bench_stat_context
{
	BENCH_STAT_CONTEXT_NONE = 0,
	BENCH_STAT_CONTEXT_OPS,
	BENCH_STAT_CONTEXT_FIRST,
	BENCH_STAT_CONTEXT_FOREIGN,
	BENCH_STAT_CONTEXT_APP_MT,
	BENCH_STAT_CONTEXT_APP_ST,
	BENCH_STAT_CONTEXT_LOCALITY
}
bench_stat_context_t;


typedef struct stats
{
	uint64_t min;
	uint64_t max;
	double mean;
	double median;
	double stddev;
	double p50;
	double p90;
	double p95;
	double p99;
	double p999;
}
stats_t;


extern bench_stat_context_t bench_stat_context;
extern size_t bench_stat_size;
extern uint64_t bench_seed_value;
extern uint32_t bench_seed_base;


#define BENCH_LOG_TAG "[bench]"
#define bench_log_info(...) alloc_do_custom_log_tagged_info(STDERR_FILENO, BENCH_LOG_TAG, __VA_ARGS__)
#define bench_log(...) bench_log_info(__VA_ARGS__)


extern void*
bench_alloc(
	size_t size,
	int zero
	);


extern void
bench_free(
	const volatile void* ptr,
	size_t size
	);


extern void*
bench_realloc(
	const volatile void* ptr,
	size_t old_size,
	size_t new_size,
	int zero
	);


extern void*
bench_alloc_aligned(
	size_t size,
	size_t alignment,
	int zero
	);


extern uint64_t
get_ns(
	void
	);


extern uint32_t
fast_rand(
	uint32_t* seed
	);


extern uint32_t
mix_u32(
	uint32_t x
	);


extern uint32_t
bench_seed_derive(
	uint32_t tag,
	uint32_t extra
	);


extern stats_t
compute_stats(
	uint64_t* samples,
	size_t n
	);


extern void
print_stats(
	const char* label,
	stats_t* s
	);


extern long
get_rss_kb(
	void
	);


extern long
get_anon_hugepages_kb(
	void
	);


extern void
bench_emit_desc(
	const char* test,
	const char* what,
	const char* measures,
	const char* method,
	const char* detects
	);


extern void
bench_emit_stat_row(
	const char* label,
	const stats_t* s
	);


extern void
bench_common_init(
	int argc,
	char** argv
	);

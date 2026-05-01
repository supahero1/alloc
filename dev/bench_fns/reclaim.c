#include "common.h"

#include <string.h>

#define RSS_END_TARGET_BYTES (64U * 1024U * 1024U)
#define RECLAIM_ALLOC_SIZE 4096


typedef struct bench_rss_args
{
	long rss_baseline;
	long rss_peak;
	size_t target_bytes;
	size_t count;
	size_t free_count;
	size_t live_bytes;
	int metadata_oom;
}
bench_rss_args_t;


int
bench_reclaim_run(
	bench_rss_args_t* args
	)
{
	size_t count = args->target_bytes / RECLAIM_ALLOC_SIZE;

	if(!count)
	{
		count = 1;
	}

	args->count = count;

	void** ptrs = calloc(count, sizeof(*ptrs));
	if(!ptrs)
	{
		args->metadata_oom = 1;
		return false;
	}

	for(size_t i = 0; i < count; ++i)
	{
		ptrs[i] = bench_alloc(RECLAIM_ALLOC_SIZE, 0);
		if(ptrs[i])
		{
			memset(ptrs[i], 0xA5, RECLAIM_ALLOC_SIZE);
		}
	}

	args->rss_peak = get_rss_kb() - args->rss_baseline;

	size_t free_count = count;
	args->free_count = free_count;

	for(size_t i = 0; i < free_count; ++i)
	{
		size_t idx = count - 1 - i;
		if(ptrs[idx])
		{
			bench_free(ptrs[idx], RECLAIM_ALLOC_SIZE);
			ptrs[idx] = NULL;
		}
	}

	size_t live_bytes = 0;
	for(size_t i = 0; i < count; ++i)
	{
		if(ptrs[i])
		{
			live_bytes += RECLAIM_ALLOC_SIZE;
		}
	}

	args->live_bytes = live_bytes;

	for(size_t i = 0; i < count; ++i)
	{
		if(ptrs[i])
		{
			bench_free(ptrs[i], RECLAIM_ALLOC_SIZE);
		}
	}

	free(ptrs);
	return true;
}


void
bench_reclaim_thread(
	void* void_arg
	)
{
	bench_reclaim_run(void_arg);
}


void
bench_section_reclaim(
	void
	)
{
	bench_emit_desc(
		"reclaim",
		"memory reclaim after heavy use",
		"rss peak vs rss end after free",
		"thread-based allocation surge followed by free",
		"metadata management and memory return to OS");

	long rss_baseline = get_rss_kb();
	long thp_baseline = get_anon_hugepages_kb();

	bench_rss_args_t args = {0};
	args.rss_baseline = rss_baseline;
	args.target_bytes = RSS_END_TARGET_BYTES;

	thread_t thread;
	thread_init(&thread, (thread_data_t){ bench_reclaim_thread, &args });
	thread_join(thread);

	if(args.metadata_oom)
	{
		bench_log("  RECLAIM size=", (size_t) RECLAIM_ALLOC_SIZE, " allocation metadata OOM");
		bench_log("BENCH|type=reclaim|size=", (size_t) RECLAIM_ALLOC_SIZE, "|error=metadata_oom");
		return;
	}

	long rss_end = get_rss_kb() - rss_baseline;
	args.rss_peak = MACRO_MAX(args.rss_peak, rss_end);

	long thp_end = get_anon_hugepages_kb() - thp_baseline;
	double end_live_ratio = args.live_bytes ? (double) (rss_end * 1024) / args.live_bytes : 0.0;

	bench_log("  RECLAIM size=", (size_t) RECLAIM_ALLOC_SIZE,
		" target=", args.target_bytes / 1024, "KB",
		" count=", args.count,
		" freed=", args.free_count,
		" peak=", args.rss_peak, "KB",
		" end=", rss_end, "KB",
		" live=", args.live_bytes / 1024, "KB",
		" ratio=", end_live_ratio,
		" thp_end=", thp_end, "KB");

	bench_log("BENCH|type=reclaim|size=", (size_t) RECLAIM_ALLOC_SIZE,
		"|target=", args.target_bytes / 1024,
		"|count=", args.count,
		"|freed=", args.free_count,
		"|peak=", args.rss_peak,
		"|end=", rss_end,
		"|live=", args.live_bytes / 1024,
		"|end_live_ratio=", end_live_ratio,
		"|thp_end=", thp_end);
}


int
main(
	int argc,
	char** argv
	)
{
	bench_common_init(argc, argv);
	bench_section_reclaim();

	return 0;
}

#include "common.h"

#include <string.h>
#include <sys/wait.h>

#define RSS_PTRS 4096
#define RSS_OPS 200000


void
bench_rss(
	size_t alloc_size
	)
{
	long rss_baseline = get_rss_kb();
	long thp_baseline = get_anon_hugepages_kb();

	void* ptrs[RSS_PTRS];
	size_t sizes[RSS_PTRS];
	memset(ptrs, 0, sizeof(ptrs));
	memset(sizes, 0, sizeof(sizes));

	uint32_t seed = bench_seed_derive(0x5515AA01U, alloc_size);
	long rss_peak = 0;

	for(int i = 0; i < RSS_PTRS; ++i)
	{
		sizes[i] = alloc_size;
		ptrs[i] = bench_alloc(alloc_size, 0);
		if(ptrs[i])
		{
			memset(ptrs[i], 0xAB, alloc_size);
		}
	}

	long rss_full = get_rss_kb() - rss_baseline;
	long thp_full = get_anon_hugepages_kb() - thp_baseline;
	rss_peak = MACRO_MAX(rss_peak, rss_full);

	for(int i = 0; i < RSS_OPS; ++i)
	{
		uint32_t r = fast_rand(&seed);
		int idx = r % RSS_PTRS;

		if(ptrs[idx])
		{
			bench_free(ptrs[idx], sizes[idx]);
			ptrs[idx] = NULL;
		}
		else
		{
			sizes[idx] = alloc_size;
			ptrs[idx] = bench_alloc(alloc_size, 0);
			if(ptrs[idx])
			{
				memset(ptrs[idx], 0xAB, alloc_size);
			}
		}

		if(!(i & 0xFFF))
		{
			long rss_now = get_rss_kb() - rss_baseline;
			rss_peak = MACRO_MAX(rss_peak, rss_now);
		}
	}

	long rss_churn = get_rss_kb() - rss_baseline;
	rss_peak = MACRO_MAX(rss_peak, rss_churn);

	size_t live_bytes = 0;
	for(int i = 0; i < RSS_PTRS; ++i)
	{
		if(ptrs[i])
		{
			live_bytes += sizes[i];
		}
	}

	for(int i = 0; i < RSS_PTRS; ++i)
	{
		if(ptrs[i])
		{
			bench_free(ptrs[i], sizes[i]);
		}
	}

	long rss_after_free = get_rss_kb() - rss_baseline;
	long thp_after_free = get_anon_hugepages_kb() - thp_baseline;

	bench_log("  RSS size=", alloc_size,
		" full=", rss_full, "KB",
		" churn=", rss_churn, "KB",
		" peak=", rss_peak, "KB",
		" after_free=", rss_after_free, "KB",
		" live=", live_bytes / 1024, "KB",
		" thp_full=", thp_full, "KB",
		" thp_free=", thp_after_free, "KB");

	bench_log("BENCH|type=rss|size=", alloc_size,
		"|full=", rss_full,
		"|churn=", rss_churn,
		"|peak=", rss_peak,
		"|after_free=", rss_after_free,
		"|live=", live_bytes / 1024,
		"|thp_full=", thp_full,
		"|thp_free=", thp_after_free);
}


void
bench_section_rss(
	size_t alloc_size
	)
{
	bench_emit_desc(
		"rss",
		"resident memory under load",
		"rss peak after free hugepage delta",
		"single deterministic rss case per process run",
		"retention purge and THP behavior");

	pid_t pid = fork();
	if(!pid)
	{
		bench_rss(alloc_size);
		_exit(0);
	}

	int status;
	waitpid(pid, &status, 0);
}


int
main(
	int argc,
	char** argv
	)
{
	bench_common_init(argc, argv);

	if(argc < 3)
	{
		return 1;
	}

	bench_section_rss(strtoul(argv[2], NULL, 0));

	return 0;
}

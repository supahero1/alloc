#include "common.h"

#include <string.h>

#ifndef _WIN32
	#include <sys/wait.h>
	#include <unistd.h>
#endif

#define FRAG_PTRS 8192
#define FRAG_ROUNDS 4


void
bench_fragmentation(
	size_t alloc_size
	)
{
	void* ptrs[FRAG_PTRS];
	size_t sizes[FRAG_PTRS];
	uint32_t seed = bench_seed_derive(0xF12A9913U, alloc_size);

	long rss_baseline = get_rss_kb();
	long thp_baseline = get_anon_hugepages_kb();

	for(int i = 0; i < FRAG_PTRS; ++i)
	{
		sizes[i] = alloc_size;
		ptrs[i] = bench_alloc(alloc_size, 0);
		if(ptrs[i])
		{
			memset(ptrs[i], 0xCC, alloc_size);
		}
	}

	for(int i = 1; i < FRAG_PTRS; i += 2)
	{
		if(ptrs[i])
		{
			bench_free(ptrs[i], sizes[i]);
			ptrs[i] = NULL;
		}
	}

	long rss_after_free_odd = get_rss_kb() - rss_baseline;

	for(int i = 1; i < FRAG_PTRS; i += 2)
	{
		sizes[i] = alloc_size * 2;
		ptrs[i] = bench_alloc(sizes[i], 0);
		if(ptrs[i])
		{
			memset(ptrs[i], 0xDD, sizes[i]);
		}
	}

	long rss_realloc = get_rss_kb() - rss_baseline;

	for(int round = 0; round < FRAG_ROUNDS; ++round)
	{
		for(int i = 0; i < FRAG_PTRS; ++i)
		{
			if(fast_rand(&seed) % 3 == 0 && ptrs[i])
			{
				bench_free(ptrs[i], sizes[i]);
				ptrs[i] = NULL;
			}
		}

		for(int i = 0; i < FRAG_PTRS; ++i)
		{
			if(!ptrs[i])
			{
				size_t size = alloc_size + fast_rand(&seed) % (alloc_size + 1);
				sizes[i] = size;
				ptrs[i] = bench_alloc(size, 0);
				if(ptrs[i])
				{
					memset(ptrs[i], 0xEE, size);
				}
			}
		}
	}

	long rss_fragmented = get_rss_kb() - rss_baseline;

	size_t live_bytes = 0;
	for(int i = 0; i < FRAG_PTRS; ++i)
	{
		if(ptrs[i])
		{
			live_bytes += sizes[i];
		}
	}

	for(int i = 0; i < FRAG_PTRS; ++i)
	{
		if(ptrs[i])
		{
			bench_free(ptrs[i], sizes[i]);
		}
	}

	long rss_end = get_rss_kb() - rss_baseline;
	long thp_fragmented = get_anon_hugepages_kb() - thp_baseline;
	double frag_ratio = rss_fragmented > 0 && live_bytes > 0
		? (double) (rss_fragmented * 1024) / live_bytes
		: 0;

	bench_log("  FRAG size=", alloc_size,
		" free_odd=", rss_after_free_odd, "KB",
		" realloc=", rss_realloc, "KB",
		" fragmented=", rss_fragmented, "KB",
		" end=", rss_end, "KB",
		" live=", live_bytes / 1024, "KB",
		" ratio=", frag_ratio,
		" thp=", thp_fragmented, "KB");

	bench_log("BENCH|type=frag|size=", alloc_size,
		"|free_odd=", rss_after_free_odd,
		"|realloc=", rss_realloc,
		"|fragmented=", rss_fragmented,
		"|end=", rss_end,
		"|live=", live_bytes / 1024,
		"|frag_ratio=", frag_ratio,
		"|thp=", thp_fragmented);
}


void
bench_section_frag(
	size_t alloc_size
	)
{
	bench_emit_desc(
		"frag",
		"mixed-size fragmentation pressure",
		"fragmented rss live ratio hugepage drift",
		"single deterministic frag case per process run",
		"external fragmentation and reuse quality");

#ifdef _WIN32
	bench_fragmentation(alloc_size);
#else
	pid_t pid = fork();
	if(!pid)
	{
		bench_fragmentation(alloc_size);
		_exit(0);
	}

	int status;
	waitpid(pid, &status, 0);
#endif
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

	size_t alloc_size = strtoul(argv[2], NULL, 0);
	bench_section_frag(alloc_size);

	return 0;
}

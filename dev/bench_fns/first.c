#include "common.h"

#include <stdio.h>

#ifndef _WIN32
	#include <sys/wait.h>
	#include <unistd.h>
#endif

#define FIRST_ALLOC_ITERS 2048


typedef struct first_alloc_arg
{
	size_t size;
	uint64_t elapsed;
}
first_alloc_arg_t;


void
first_alloc_thread(
	void* arg
	)
{
	first_alloc_arg_t* fa = arg;

	uint64_t t0 = get_ns();
	void* p = bench_alloc(fa->size, 0);
	uint64_t t1 = get_ns();
	fa->elapsed = t1 - t0;

	bench_free(p, fa->size);
}


stats_t
bench_first_alloc(
	size_t alloc_size
	)
{
	uint64_t* samples = malloc(sizeof(uint64_t) * FIRST_ALLOC_ITERS);

	for(int i = 0; i < FIRST_ALLOC_ITERS; ++i)
	{
		first_alloc_arg_t fa = { .size = alloc_size };
		thread_t t;
		thread_init(&t, (thread_data_t){ first_alloc_thread, &fa });
		thread_join(t);
		samples[i] = fa.elapsed;
	}

	stats_t s = compute_stats(samples, FIRST_ALLOC_ITERS);
	free(samples);
	return s;
}


void
bench_size_label(
	size_t size,
	char* buf,
	size_t buf_size
	)
{
	if(size % (1024 * 1024) == 0)
	{
		snprintf(buf, buf_size, "%zuMiB", size / (1024 * 1024));
		return;
	}

	if(size % 1024 == 0)
	{
		snprintf(buf, buf_size, "%zuKiB", size / 1024);
		return;
	}

	snprintf(buf, buf_size, "%zuB", size);
}


void
bench_section_first_alloc_size(
	size_t alloc_size
	)
{
	bench_emit_desc(
		"first",
		"cold allocation on fresh thread",
		"thread first-touch allocation latency",
		"spawn thread allocate once then join",
		"thread init and TLS startup cost");

	bench_stat_context = BENCH_STAT_CONTEXT_FIRST;
	bench_stat_size = alloc_size;

	char label[64];
	char size_buf[24];
	bench_size_label(alloc_size, size_buf, sizeof(size_buf));
	snprintf(label, sizeof(label), "first alloc (%s)", size_buf);

#ifdef _WIN32
	stats_t s = bench_first_alloc(alloc_size);
	print_stats(label, &s);
#else
	pid_t pid = fork();
	if(!pid)
	{
		stats_t s = bench_first_alloc(alloc_size);
		print_stats(label, &s);
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

	if(argc < 3)
	{
		return 1;
	}

	size_t alloc_size = strtoul(argv[2], NULL, 0);
	bench_section_first_alloc_size(alloc_size);

	return 0;
}

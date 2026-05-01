#include "common.h"

#include <stdio.h>
#include <sys/wait.h>

#define BENCH_LOCALITY_TARGET_BYTES (32 * 1024 * 1024)
#define BENCH_LOCALITY_REFERENCE_NODE_SIZE 1024
#define BENCH_LOCALITY_NODE_COUNT (BENCH_LOCALITY_TARGET_BYTES / BENCH_LOCALITY_REFERENCE_NODE_SIZE)


typedef struct ll_node
{
	struct ll_node* next;
	char pad[];
}
ll_node_t;


stats_t
bench_linked_list(
	size_t node_size,
	int node_count,
	int walk_iters
	)
{
	if(node_size < sizeof(ll_node_t))
	{
		node_size = sizeof(ll_node_t);
	}

	ll_node_t* head = bench_alloc(node_size, 0);
	head->next = NULL;

	ll_node_t* tail = head;

	for(int i = 1; i < node_count; ++i)
	{
		ll_node_t* n = bench_alloc(node_size, 0);
		n->next = NULL;
		tail->next = n;
		tail = n;
	}

	uint64_t* samples = malloc(sizeof(uint64_t) * walk_iters);

	for(int w = 0; w < walk_iters; ++w)
	{
		uint64_t t0 = get_ns();

		volatile int count = 0;
		ll_node_t* cur = head;
		while(cur)
		{
			++count;
			cur = cur->next;
		}

		uint64_t t1 = get_ns();
		samples[w] = t1 - t0;
		(void) count;
	}

	stats_t s = compute_stats(samples, walk_iters);
	free(samples);

	ll_node_t* cur = head;
	while(cur)
	{
		ll_node_t* next = cur->next;
		bench_free(cur, node_size);
		cur = next;
	}

	return s;
}


void
bench_section_linked_list_case(
	size_t node_size
	)
{
	bench_emit_desc(
		"locality",
		"pointer-chasing walk over linearly-built lists",
		"walk latency and adjacency distance",
		"allocate list nodes sequentially then traverse repeatedly",
		"continuity and arena boundary effects");

	int walk_iters = 500;
	int node_count = BENCH_LOCALITY_NODE_COUNT;
	bench_stat_context = BENCH_STAT_CONTEXT_LOCALITY;
	bench_stat_size = node_size;

	pid_t pid = fork();
	if(!pid)
	{
		char label[64];
		snprintf(label, sizeof(label), "ll walk %zuB", node_size);
		stats_t s = bench_linked_list(node_size, node_count, walk_iters);
		print_stats(label, &s);
		_exit(0);
	}

	int status;
	waitpid(pid, &status, 0);

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

	size_t node_size = strtoul(argv[2], NULL, 0);
	bench_section_linked_list_case(node_size);

	return 0;
}

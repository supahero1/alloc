#include "app_mix.h"


void
bench_section_app_mix_mt(
	void
	)
{
	bench_emit_desc(
		"app_mt",
		"multithreaded mixed application workload",
		"throughput runtime alloc free realloc latency tails",
		"local caches plus shared handoff slots",
		"contention behavior and mixed-path balance");

	bench_stat_context = BENCH_STAT_CONTEXT_APP_MT;
	bench_stat_size = 0;

	bench_app_mix("mt", "app mt", APP_MIX_THREADS, APP_MIX_TARGET_RUNTIME_NS, 1, 0xA11C0000U);

	bench_stat_context = BENCH_STAT_CONTEXT_NONE;
}


int
main(
	int argc,
	char** argv
	)
{
	bench_common_init(argc, argv);
	bench_section_app_mix_mt();

	return 0;
}

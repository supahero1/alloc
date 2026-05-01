#include "app_mix.h"


void
bench_section_app_mix_st(
	void
	)
{
	bench_emit_desc(
		"app_st",
		"single-thread mixed application workload",
		"throughput runtime alloc free realloc latency tails",
		"single worker no shared handoff slots long deterministic run",
		"single-thread path balance without scheduler noise");

	bench_stat_context = BENCH_STAT_CONTEXT_APP_ST;
	bench_stat_size = 0;

	bench_app_mix("st", "app st", 1, APP_MIX_TARGET_RUNTIME_NS, 0, 0xA11C1000U);

	bench_stat_context = BENCH_STAT_CONTEXT_NONE;
}


int
main(
	int argc,
	char** argv
	)
{
	bench_common_init(argc, argv);
	bench_section_app_mix_st();

	return 0;
}

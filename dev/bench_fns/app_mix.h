#pragma once

#include "common.h"


#define APP_MIX_THREADS 8
#define APP_MIX_TARGET_RUNTIME_NS ((uint64_t) 10 * 1000000000)
#define APP_MIX_STOP_POLL_MASK 1023
#define APP_MIX_LOCAL_SLOTS 192
#define APP_MIX_SHARED_SLOTS 512
#define APP_MIX_MAX_SAMPLES ((size_t) 1 << 18)
#define APP_MIX_REGULAR_HIGH_MAX 65536


typedef struct app_mix_slot
{
	sync_mtx_t mutex;
	void* ptr;
	size_t size;
}
app_mix_slot_t;


typedef struct app_mix_ctx
{
	app_mix_slot_t shared[APP_MIX_SHARED_SLOTS];
	uint64_t _Atomic alloc_ops;
	uint64_t _Atomic free_ops;
	uint64_t _Atomic realloc_ops;
	uint64_t _Atomic foreign_handoffs;
	uint32_t _Atomic alloc_n;
	uint32_t _Atomic free_n;
	uint32_t _Atomic realloc_n;
	uint32_t _Atomic foreign_free_n;
	uint32_t _Atomic stop;
	uint64_t deadline_ns;
	uint64_t* alloc_samples;
	uint64_t* free_samples;
	uint64_t* realloc_samples;
	uint64_t* foreign_free_samples;
}
app_mix_ctx_t;


typedef struct app_mix_arg
{
	app_mix_ctx_t* ctx;
	int shared_enabled;
	uint32_t seed;
	uint64_t executed_ops;
}
app_mix_arg_t;


extern void
bench_app_mix(
	const char* variant,
	const char* stats_prefix,
	int threads,
	uint64_t runtime_ns,
	int shared_enabled,
	uint32_t seed_tag
	);

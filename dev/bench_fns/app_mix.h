#pragma once

#include "common.h"


#define APP_MIX_THREADS 8
#define APP_MIX_TARGET_RUNTIME_NS ((uint64_t) 10 * 1000000000)
#define APP_MIX_STOP_POLL_MASK 1023
#define APP_MIX_LOCAL_SLOTS 192
#define APP_MIX_SHARED_SLOTS 512
#define APP_MIX_RESERVOIR_SLOTS ((size_t) 1 << 17)
#define APP_MIX_TIMED_MASK 7
#define APP_MIX_REGULAR_HIGH_MAX 65536


typedef struct attr_aligned(64) app_mix_slot
{
	sync_mtx_t mutex;
	void* ptr;
	size_t size;
}
app_mix_slot_t;


typedef struct app_mix_ctx
{
	app_mix_slot_t shared[APP_MIX_SHARED_SLOTS];
	uint32_t _Atomic stop;
	uint64_t deadline_ns;
}
app_mix_ctx_t;


typedef enum app_mix_kind
{
	APP_MIX_KIND_ALLOC			= 0,
	APP_MIX_KIND_FREE			= 1,
	APP_MIX_KIND_REALLOC		= 2,
	APP_MIX_KIND_FOREIGN_FREE	= 3,
	MACRO_ENUM_BITS(APP_MIX_KIND)
}
app_mix_kind_t;


typedef struct app_mix_reservoir
{
	uint64_t* samples;
	uint64_t seen;
	size_t used;
}
app_mix_reservoir_t;


typedef struct attr_aligned(64) app_mix_arg
{
	app_mix_ctx_t* ctx;
	int shared_enabled;
	uint32_t seed;
	uint32_t sample_seed;
	uint64_t executed_ops;
	uint64_t alloc_ops;
	uint64_t free_ops;
	uint64_t realloc_ops;
	uint64_t handoffs;
	app_mix_reservoir_t reservoirs[APP_MIX_KIND__COUNT];
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

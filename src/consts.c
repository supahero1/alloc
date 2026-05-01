/*
 *   Copyright 2026 Franciszek Balcerak
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <alloc/log.h>
#include <alloc/attr.h>
#include <alloc/huge.h>
#include <alloc/sync.h>
#include <alloc/arena.h>
#include <alloc/btree.h>
#include <alloc/debug.h>
#include <alloc/macro.h>
#include <alloc/types.h>
#include <alloc/atomic.h>
#include <alloc/consts.h>
#include <alloc/report.h>
#include <alloc/threads.h>
#include <alloc/platform.h>
#include <alloc/bootstrap.h>

#include <valgrind/valgrind.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if !defined(ALLOC_RELEASE) && __has_include(<valgrind/valgrind.h>)
	#define ALLOC_RUNNING_ON_VALGRIND() (RUNNING_ON_VALGRIND)
#else
	#define ALLOC_RUNNING_ON_VALGRIND() (0)
#endif


alloc_consts_t alloc_consts;


const alloc_consts_t alloc_defaults =
{
	.red_zone =
	{
		.size = ALLOC_RED_ZONE_SIZE
	},
	.log =
	{
		.verbosity =
#ifdef ALLOC_RELEASE
			ALLOC_LOG_VERBOSITY_WARNING
#else
			ALLOC_LOG_VERBOSITY_INFO
#endif
			,
		.fd = STDERR_FILENO,
		.dump_consts = 0
	},
	.report =
	{
		.fd = STDERR_FILENO,
		.period_ms = 1000,
		.enable = 0,
		.per_thread_enable = 0
	},
	.bootstrap =
	{
		.size = (alloc_t) 1 << 20,
		.reserve_size = (alloc_t) 1 << 12
	},
	.huge =
	{
		.calibrate_ms = 50,
		.threshold_ms = 1000,
		.slots = 1024
	},
	.arena =
	{
		.size = (alloc_t) 1 << 19,
		.virtual_capacity = (alloc_t) 1 << (MACRO_BITS(void*) > 32 ? 14 : 10),
		.max_free_per_thread = 4,
		.preallocate = 16,
		.commit_batch = 8,
		.thp_enable = 1,
		.tail_decommit_enable = 1,
		.tail_decommit_trigger_div = 4,
		.tail_decommit_amount_div = 2,
		.prefix_thp_enable = 0,
		.prefix_tail_decommit_enable = 0,
		.prefix_tail_decommit_trigger_div = 4,
		.prefix_tail_decommit_amount_div = 2
	},
	.slab =
	{
		.small = (alloc_t) 1 << 13,
		.medium = (alloc_t) 1 << 15,
		.large = (alloc_t) 1 << 18,
		.small_min_blocks = 64,
		.medium_min_blocks = 16,
		.large_min_blocks = 2,
		.max_free_per_handle = 4
	},
	.tcache =
	{
		.max_per_class = 32
	},
	.tcb =
	{
		.capacity = 65536,
		.preallocate = 16,
		.commit_batch = 8
	}
};


typedef struct alloc_env
{
	alloc_t red_zone_size;
	alloc_t log_verbosity;
	alloc_t log_fd;
	alloc_t log_dump_consts;
	alloc_t report_fd;
	alloc_t report_period_ms;
	alloc_t report_enable;
	alloc_t report_per_thread_enable;
	alloc_t bootstrap_size;
	alloc_t bootstrap_reserve_size;
	alloc_t huge_calibrate_ms;
	alloc_t huge_threshold_ms;
	alloc_t huge_slots;
	alloc_t arena_size;
	alloc_t arena_virtual_capacity;
	alloc_t arena_max_free_per_thread;
	alloc_t arena_preallocate;
	alloc_t arena_commit_batch;
	alloc_t arena_thp_enable;
	alloc_t arena_tail_decommit_enable;
	alloc_t arena_tail_decommit_trigger_div;
	alloc_t arena_tail_decommit_amount_div;
	alloc_t arena_prefix_thp_enable;
	alloc_t arena_prefix_tail_decommit_enable;
	alloc_t arena_prefix_tail_decommit_trigger_div;
	alloc_t arena_prefix_tail_decommit_amount_div;
	alloc_t slab_small;
	alloc_t slab_medium;
	alloc_t slab_large;
	alloc_t slab_small_min_blocks;
	alloc_t slab_medium_min_blocks;
	alloc_t slab_large_min_blocks;
	alloc_t slab_max_free_per_handle;
	alloc_t tcache_max_per_class;
	alloc_t tcb_capacity;
	alloc_t tcb_preallocate;
	alloc_t tcb_commit_batch;

	alloc_t red_zone_size_set:1;
	alloc_t log_verbosity_set:1;
	alloc_t log_fd_set:1;
	alloc_t log_dump_consts_set:1;
	alloc_t report_fd_set:1;
	alloc_t report_period_ms_set:1;
	alloc_t report_enable_set:1;
	alloc_t report_per_thread_enable_set:1;
	alloc_t bootstrap_size_set:1;
	alloc_t bootstrap_reserve_size_set:1;
	alloc_t huge_calibrate_ms_set:1;
	alloc_t huge_threshold_ms_set:1;
	alloc_t huge_slots_set:1;
	alloc_t arena_size_set:1;
	alloc_t arena_virtual_capacity_set:1;
	alloc_t arena_max_free_per_thread_set:1;
	alloc_t arena_preallocate_set:1;
	alloc_t arena_commit_batch_set:1;
	alloc_t arena_thp_enable_set:1;
	alloc_t arena_tail_decommit_enable_set:1;
	alloc_t arena_tail_decommit_trigger_div_set:1;
	alloc_t arena_tail_decommit_amount_div_set:1;
	alloc_t arena_prefix_thp_enable_set:1;
	alloc_t arena_prefix_tail_decommit_enable_set:1;
	alloc_t arena_prefix_tail_decommit_trigger_div_set:1;
	alloc_t arena_prefix_tail_decommit_amount_div_set:1;
	alloc_t slab_small_set:1;
	alloc_t slab_medium_set:1;
	alloc_t slab_large_set:1;
	alloc_t slab_small_min_blocks_set:1;
	alloc_t slab_medium_min_blocks_set:1;
	alloc_t slab_large_min_blocks_set:1;
	alloc_t slab_max_free_per_handle_set:1;
	alloc_t tcache_max_per_class_set:1;
	alloc_t tcb_capacity_set:1;
	alloc_t tcb_preallocate_set:1;
	alloc_t tcb_commit_batch_set:1;

	alloc_t red_zone_size_ignored:1;
	alloc_t log_verbosity_ignored:1;
	alloc_t log_fd_ignored:1;
	alloc_t log_dump_consts_ignored:1;
	alloc_t report_fd_ignored:1;
	alloc_t report_period_ms_ignored:1;
	alloc_t report_enable_ignored:1;
	alloc_t report_per_thread_enable_ignored:1;
	alloc_t bootstrap_size_ignored:1;
	alloc_t bootstrap_reserve_size_ignored:1;
	alloc_t huge_calibrate_ms_ignored:1;
	alloc_t huge_threshold_ms_ignored:1;
	alloc_t huge_slots_ignored:1;
	alloc_t arena_size_ignored:1;
	alloc_t arena_virtual_capacity_ignored:1;
	alloc_t arena_max_free_per_thread_ignored:1;
	alloc_t arena_preallocate_ignored:1;
	alloc_t arena_commit_batch_ignored:1;
	alloc_t arena_thp_enable_ignored:1;
	alloc_t arena_tail_decommit_enable_ignored:1;
	alloc_t arena_tail_decommit_trigger_div_ignored:1;
	alloc_t arena_tail_decommit_amount_div_ignored:1;
	alloc_t arena_prefix_thp_enable_ignored:1;
	alloc_t arena_prefix_tail_decommit_enable_ignored:1;
	alloc_t arena_prefix_tail_decommit_trigger_div_ignored:1;
	alloc_t arena_prefix_tail_decommit_amount_div_ignored:1;
	alloc_t slab_small_ignored:1;
	alloc_t slab_medium_ignored:1;
	alloc_t slab_large_ignored:1;
	alloc_t slab_small_min_blocks_ignored:1;
	alloc_t slab_medium_min_blocks_ignored:1;
	alloc_t slab_large_min_blocks_ignored:1;
	alloc_t slab_max_free_per_handle_ignored:1;
	alloc_t tcache_max_per_class_ignored:1;
	alloc_t tcb_capacity_ignored:1;
	alloc_t tcb_preallocate_ignored:1;
	alloc_t tcb_commit_batch_ignored:1;

	alloc_t bootstrap_size_rounded:1;
	alloc_t huge_slots_rounded:1;
	alloc_t arena_size_rounded:1;
	alloc_t arena_virtual_capacity_rounded:1;
	alloc_t slab_small_rounded:1;
	alloc_t slab_medium_rounded:1;
	alloc_t slab_large_rounded:1;
	alloc_t slab_small_min_blocks_rounded:1;
	alloc_t slab_medium_min_blocks_rounded:1;
	alloc_t slab_large_min_blocks_rounded:1;
}
alloc_env_t;


typedef struct alloc_unknown_param
{
	const char* str;
	const char* str_end;
}
alloc_unknown_param_t;


alloc_unknown_param_t alloc_unknown_params[ALLOC_MAX_UNKNOWN_PARAMS];
alloc_unknown_param_t* alloc_unknown_param = alloc_unknown_params;


alloc_env_t
alloc_parse_env(
	void
	)
{
	alloc_env_t env = {0};

	const char* str = alloc_get_env(ALLOC_CONFIG_NAME);
	if(!str)
	{
		return env;
	}

	const char* str_start = str;
	const char* str_end = str + strlen(str);

	while(str < str_end)
	{
		if(0)
		{
		}

#define ALLOC_PARSE(name)									\
		else if(!strncmp(str, #name "=", sizeof(#name)))	\
		{													\
			str += sizeof(#name);							\
			env.name = strtoull(str, (char**) &str, 0);		\
			env.name##_set = 1;								\
		}

		ALLOC_PARSE(red_zone_size)
		ALLOC_PARSE(log_verbosity)
		ALLOC_PARSE(log_fd)
		ALLOC_PARSE(log_dump_consts)
		ALLOC_PARSE(report_fd)
		ALLOC_PARSE(report_period_ms)
		ALLOC_PARSE(report_enable)
		ALLOC_PARSE(report_per_thread_enable)
		ALLOC_PARSE(bootstrap_size)
		ALLOC_PARSE(bootstrap_reserve_size)
		ALLOC_PARSE(huge_calibrate_ms)
		ALLOC_PARSE(huge_threshold_ms)
		ALLOC_PARSE(huge_slots)
		ALLOC_PARSE(arena_size)
		ALLOC_PARSE(arena_virtual_capacity)
		ALLOC_PARSE(arena_max_free_per_thread)
		ALLOC_PARSE(arena_preallocate)
		ALLOC_PARSE(arena_commit_batch)
		ALLOC_PARSE(arena_thp_enable)
		ALLOC_PARSE(arena_tail_decommit_enable)
		ALLOC_PARSE(arena_tail_decommit_trigger_div)
		ALLOC_PARSE(arena_tail_decommit_amount_div)
		ALLOC_PARSE(arena_prefix_thp_enable)
		ALLOC_PARSE(arena_prefix_tail_decommit_enable)
		ALLOC_PARSE(arena_prefix_tail_decommit_trigger_div)
		ALLOC_PARSE(arena_prefix_tail_decommit_amount_div)
		ALLOC_PARSE(slab_small)
		ALLOC_PARSE(slab_medium)
		ALLOC_PARSE(slab_large)
		ALLOC_PARSE(slab_small_min_blocks)
		ALLOC_PARSE(slab_medium_min_blocks)
		ALLOC_PARSE(slab_large_min_blocks)
		ALLOC_PARSE(slab_max_free_per_handle)
		ALLOC_PARSE(tcache_max_per_class)
		ALLOC_PARSE(tcb_capacity)
		ALLOC_PARSE(tcb_preallocate)
		ALLOC_PARSE(tcb_commit_batch)

#undef ALLOC_PARSE

		else
		{
			while(str < str_end && *str != '=' && *str != ',') ++str;

			if(alloc_unknown_param < alloc_unknown_params + MACRO_ARRAY_LEN(alloc_unknown_params))
			{
				alloc_unknown_param->str = str_start;
				alloc_unknown_param->str_end = str;
				++alloc_unknown_param;
			}

			while(str < str_end && *str != ',') ++str;
		}

		if(*str == ',')
		{
			++str;
			str_start = str;
		}
	}

	return env;
}


void
alloc_validate_env(
	alloc_env_t* env
	)
{
	assert_not_null(env);

#define ALLOC_IGNORE(name)		\
	env->name##_set = 0;		\
	env->name##_ignored = 1;	\

#define ALLOC_VALIDATE(name, cond)		\
	if(env->name##_set && !(cond))		\
	{									\
		ALLOC_IGNORE(name)				\
	}									\
	if(env->name##_set) {}				\

#define ALLOC_ENSURE_PO2(name)									\
	if(env->name##_set && !MACRO_IS_POWER_OF_2(env->name))		\
	{															\
		env->name = MACRO_NEXT_OR_EQUAL_POWER_OF_2(env->name);	\
		env->name##_rounded = 1;								\
	}

	ALLOC_VALIDATE(red_zone_size, env->red_zone_size <= (alloc_t) 1 << 16)
	ALLOC_VALIDATE(log_verbosity, env->log_verbosity < ALLOC_LOG_VERBOSITY__COUNT)
	ALLOC_VALIDATE(log_fd, env->log_fd <= INT32_MAX)
	ALLOC_VALIDATE(log_dump_consts, env->log_dump_consts <= 1)
	ALLOC_VALIDATE(report_fd, env->report_fd <= INT32_MAX)
	ALLOC_VALIDATE(report_period_ms, env->report_period_ms && env->report_period_ms <= 86400000)
	ALLOC_VALIDATE(report_enable, env->report_enable <= 1)
	ALLOC_VALIDATE(report_per_thread_enable, env->report_per_thread_enable <= 1)
	ALLOC_VALIDATE(bootstrap_size, env->bootstrap_size && env->bootstrap_size <= (alloc_t) 1 << 30)
	ALLOC_ENSURE_PO2(bootstrap_size)
	ALLOC_VALIDATE(bootstrap_reserve_size, env->bootstrap_reserve_size)
	ALLOC_VALIDATE(huge_calibrate_ms, env->huge_calibrate_ms && env->huge_calibrate_ms <= 60000)
	ALLOC_VALIDATE(huge_threshold_ms, env->huge_threshold_ms && env->huge_threshold_ms <= 86400000)
	ALLOC_VALIDATE(huge_slots, env->huge_slots && env->huge_slots <= (alloc_t) 1 << 20)
	ALLOC_ENSURE_PO2(huge_slots)
	ALLOC_VALIDATE(arena_size, env->arena_size && env->arena_size <= (alloc_t) 1 << 30)
	ALLOC_ENSURE_PO2(arena_size)
	ALLOC_VALIDATE(arena_virtual_capacity, env->arena_virtual_capacity && env->arena_virtual_capacity <= (alloc_t) 1 << 20)
	ALLOC_ENSURE_PO2(arena_virtual_capacity)
	ALLOC_VALIDATE(arena_max_free_per_thread, env->arena_max_free_per_thread && env->arena_max_free_per_thread <= ALLOC_MAX_ARENA_FREE_PER_THREAD)
	ALLOC_VALIDATE(arena_preallocate, env->arena_preallocate && env->arena_preallocate <= (alloc_t) 1 << 20)
	ALLOC_VALIDATE(arena_commit_batch, env->arena_commit_batch && env->arena_commit_batch <= (alloc_t) 1 << 16)
	ALLOC_VALIDATE(arena_thp_enable, env->arena_thp_enable <= 1)
	ALLOC_VALIDATE(arena_tail_decommit_enable, env->arena_tail_decommit_enable <= 1)
	ALLOC_VALIDATE(arena_tail_decommit_trigger_div, env->arena_tail_decommit_trigger_div >= 2 && env->arena_tail_decommit_trigger_div <= 64)
	ALLOC_VALIDATE(arena_tail_decommit_amount_div, env->arena_tail_decommit_amount_div >= 2 && env->arena_tail_decommit_amount_div <= 64)
	ALLOC_VALIDATE(arena_prefix_thp_enable, env->arena_prefix_thp_enable <= 1)
	ALLOC_VALIDATE(arena_prefix_tail_decommit_enable, env->arena_prefix_tail_decommit_enable <= 1)
	ALLOC_VALIDATE(arena_prefix_tail_decommit_trigger_div, env->arena_prefix_tail_decommit_trigger_div >= 2 && env->arena_prefix_tail_decommit_trigger_div <= 64)
	ALLOC_VALIDATE(arena_prefix_tail_decommit_amount_div, env->arena_prefix_tail_decommit_amount_div >= 2 && env->arena_prefix_tail_decommit_amount_div <= 64)
	ALLOC_VALIDATE(slab_small, env->slab_small && env->slab_small <= (alloc_t) 1 << 20)
	ALLOC_ENSURE_PO2(slab_small)
	ALLOC_VALIDATE(slab_medium, env->slab_medium && env->slab_medium <= (alloc_t) 1 << 24)
	ALLOC_ENSURE_PO2(slab_medium)
	ALLOC_VALIDATE(slab_large, env->slab_large && env->slab_large <= (alloc_t) 1 << 26)
	ALLOC_ENSURE_PO2(slab_large)
	ALLOC_VALIDATE(slab_small_min_blocks, env->slab_small_min_blocks && env->slab_small_min_blocks <= (alloc_t) 1 << 16)
	ALLOC_ENSURE_PO2(slab_small_min_blocks)
	ALLOC_VALIDATE(slab_medium_min_blocks, env->slab_medium_min_blocks && env->slab_medium_min_blocks <= (alloc_t) 1 << 16)
	ALLOC_ENSURE_PO2(slab_medium_min_blocks)
	ALLOC_VALIDATE(slab_large_min_blocks, env->slab_large_min_blocks && env->slab_large_min_blocks <= (alloc_t) 1 << 16)
	ALLOC_ENSURE_PO2(slab_large_min_blocks)
	ALLOC_VALIDATE(slab_max_free_per_handle, env->slab_max_free_per_handle <= (alloc_t) 1 << 16)
	ALLOC_VALIDATE(tcache_max_per_class, !env->tcache_max_per_class || (env->tcache_max_per_class <= ALLOC_MAX_TCACHE_CAPACITY && MACRO_IS_POWER_OF_2(env->tcache_max_per_class)))
	ALLOC_VALIDATE(tcb_capacity, env->tcb_capacity && env->tcb_capacity <= (alloc_t) 1 << sizeof(alloc_tid_t))
	ALLOC_VALIDATE(tcb_preallocate, env->tcb_preallocate && env->tcb_preallocate <= (alloc_t) 1 << 20)
	ALLOC_VALIDATE(tcb_commit_batch, env->tcb_commit_batch && env->tcb_commit_batch <= (alloc_t) 1 << 16)

	if(env->slab_small_set && env->slab_medium_set && env->slab_medium < env->slab_small)
	{
		ALLOC_IGNORE(slab_medium)
	}
	if(env->slab_medium_set && env->slab_large_set && env->slab_large < env->slab_medium)
	{
		ALLOC_IGNORE(slab_large)
	}

#undef ALLOC_ENSURE_PO2
#undef ALLOC_VALIDATE
#undef ALLOC_IGNORE
}


void
alloc_log_unknown(
	alloc_env_t* env
	)
{
	assert_not_null(env);

	for(alloc_unknown_param_t* param = alloc_unknown_params; param < alloc_unknown_param; ++param)
	{
		size_t len = param->str_end - param->str;
		char name[len + 1];
		memcpy(name, param->str, len);
		name[len] = '\0';

		alloc_log_warning("log_unknown(): ignoring unknown parameter name=", name);
	}

	if(alloc_unknown_param == alloc_unknown_params + MACRO_ARRAY_LEN(alloc_unknown_params))
	{
		alloc_log_warning("log_unknown(): reached the limit of " MACRO_STR(ALLOC_MAX_UNKNOWN_PARAMS) " unknown parameters");
	}
}


void
alloc_log_ignored(
	const alloc_env_t* env
	)
{
#ifdef ALLOC_RELEASE
	if(env->red_zone_size_set && env->red_zone_size)
	{
		alloc_log_warning("log_ignored(): red zones are disabled in release builds, ignoring red_zone_size=", env->red_zone_size);
	}

	if(env->log_verbosity_set && env->log_verbosity >= ALLOC_LOG_VERBOSITY_INFO)
	{
		alloc_log_warning("log_ignored(): only errors and warnings are logged in release builds, ignoring log_verbosity=", env->log_verbosity);
	}
#endif

#define ALLOC_LOG_IGNORED(name)																				\
	if(env->name##_ignored)																					\
	{																										\
		alloc_log_warning("log_ignored(): ignored invalid configuration value for "	#name "=", env->name);	\
	}

	ALLOC_LOG_IGNORED(red_zone_size)
	ALLOC_LOG_IGNORED(log_verbosity)
	ALLOC_LOG_IGNORED(log_fd)
	ALLOC_LOG_IGNORED(log_dump_consts)
	ALLOC_LOG_IGNORED(report_fd)
	ALLOC_LOG_IGNORED(report_period_ms)
	ALLOC_LOG_IGNORED(report_enable)
	ALLOC_LOG_IGNORED(report_per_thread_enable)
	ALLOC_LOG_IGNORED(bootstrap_size)
	ALLOC_LOG_IGNORED(bootstrap_reserve_size)
	ALLOC_LOG_IGNORED(huge_calibrate_ms)
	ALLOC_LOG_IGNORED(huge_threshold_ms)
	ALLOC_LOG_IGNORED(huge_slots)
	ALLOC_LOG_IGNORED(arena_size)
	ALLOC_LOG_IGNORED(arena_virtual_capacity)
	ALLOC_LOG_IGNORED(arena_max_free_per_thread)
	ALLOC_LOG_IGNORED(arena_preallocate)
	ALLOC_LOG_IGNORED(arena_commit_batch)
	ALLOC_LOG_IGNORED(arena_thp_enable)
	ALLOC_LOG_IGNORED(arena_tail_decommit_enable)
	ALLOC_LOG_IGNORED(arena_tail_decommit_trigger_div)
	ALLOC_LOG_IGNORED(arena_tail_decommit_amount_div)
	ALLOC_LOG_IGNORED(arena_prefix_thp_enable)
	ALLOC_LOG_IGNORED(arena_prefix_tail_decommit_enable)
	ALLOC_LOG_IGNORED(arena_prefix_tail_decommit_trigger_div)
	ALLOC_LOG_IGNORED(arena_prefix_tail_decommit_amount_div)
	ALLOC_LOG_IGNORED(slab_small)
	ALLOC_LOG_IGNORED(slab_medium)
	ALLOC_LOG_IGNORED(slab_large)
	ALLOC_LOG_IGNORED(slab_small_min_blocks)
	ALLOC_LOG_IGNORED(slab_medium_min_blocks)
	ALLOC_LOG_IGNORED(slab_large_min_blocks)
	ALLOC_LOG_IGNORED(slab_max_free_per_handle)
	ALLOC_LOG_IGNORED(tcache_max_per_class)
	ALLOC_LOG_IGNORED(tcb_capacity)
	ALLOC_LOG_IGNORED(tcb_preallocate)
	ALLOC_LOG_IGNORED(tcb_commit_batch)

#undef ALLOC_LOG_IGNORED
}


void
alloc_log_rounded(
	const alloc_env_t* env
	)
{
#define ALLOC_LOG_ROUNDED(name)											\
	if(env->name##_rounded)												\
	{																	\
		alloc_log_warning(												\
			"log_rounded(): rounded invalid configuration value for "	\
			#name " to the next or equal power of 2");					\
	}

	ALLOC_LOG_ROUNDED(bootstrap_size)
	ALLOC_LOG_ROUNDED(huge_slots)
	ALLOC_LOG_ROUNDED(arena_size)
	ALLOC_LOG_ROUNDED(arena_virtual_capacity)
	ALLOC_LOG_ROUNDED(slab_small)
	ALLOC_LOG_ROUNDED(slab_medium)
	ALLOC_LOG_ROUNDED(slab_large)
	ALLOC_LOG_ROUNDED(slab_small_min_blocks)
	ALLOC_LOG_ROUNDED(slab_medium_min_blocks)
	ALLOC_LOG_ROUNDED(slab_large_min_blocks)

#undef ALLOC_LOG_ROUNDED
}


void
alloc_apply_slab_geometry(
	void
	)
{
	static_assert(ALLOC_MAX_SLAB_CLASSES >= 3);

	alloc_consts.slab.class_count = 3;
	alloc_consts.slab.class_span[0] = 1;
	alloc_consts.slab.class_span[1] = alloc_consts.slab.medium >> alloc_consts.slab.small_shift;
	alloc_consts.slab.class_span[2] = alloc_consts.slab.large >> alloc_consts.slab.small_shift;

	alloc_consts.slab.class_span_shift[0] = 0;
	alloc_consts.slab.class_span_shift[1] = alloc_consts.slab.medium_shift - alloc_consts.slab.small_shift;
	alloc_consts.slab.class_span_shift[2] = alloc_consts.slab.large_shift - alloc_consts.slab.small_shift;
}


alloc_t
alloc_gcd(
	alloc_t a,
	alloc_t b
	)
{
	while(b)
	{
		alloc_t temp = b;
		b = a % b;
		a = temp;
	}

	return a;
}


bool
alloc_create_handle(
	const alloc_handle_info_t* info,
	alloc_handle_t* handle
	)
{
	assert_not_null(handle);
	assert_not_null(info);
	assert_gt(info->alloc_size, 0);

	assert_neq(info->alignment, 0);
	assert_true(MACRO_IS_POWER_OF_2(info->alignment));
	alloc_t alignment = MACRO_MIN(info->alignment, alloc_consts.page.size);

	if(info->alloc_size > alloc_consts.slab.virtual_cutoff)
	{
		return false;
	}

	alloc_t physical_size = info->alloc_size;
	alloc_t front_red_zone = 0;

	if(alloc_consts.red_zone.size)
	{
		front_red_zone = MACRO_ALIGN_UP(alloc_consts.red_zone.size, alignment - 1);
		physical_size += front_red_zone + alloc_consts.red_zone.size;

		alloc_t subsequent_alignment = alloc_gcd(alignment, info->alloc_size);
		physical_size = MACRO_ALIGN_UP(physical_size, subsequent_alignment - 1);
	}

	for(alloc_t slab_class = 0; slab_class < alloc_consts.slab.class_count; ++slab_class)
	{
		alloc_t base_span = alloc_consts.slab.class_span[slab_class];
		hard_assert_neq(base_span, 0);
		hard_assert_true(MACRO_IS_POWER_OF_2(base_span));

		if(base_span > alloc_consts.arena.slab_count)
		{
			break;
		}

		alloc_t slab_size = base_span << alloc_consts.slab.small_shift;
		if(slab_size < alignment)
		{
			continue;
		}

		alloc_t padding = front_red_zone;
		alloc_t usable = slab_size - padding;
		alloc_t misalignment = usable % alignment;
		if(misalignment)
		{
			padding += alignment - misalignment;
		}
		alloc_t count = (slab_size - padding) / physical_size;
		alloc_t effective_min;
		if(slab_class == 0)
		{
			effective_min = alloc_consts.slab.small_min_blocks;
		}
		else if(slab_class == 1)
		{
			effective_min = alloc_consts.slab.medium_min_blocks;
		}
		else
		{
			effective_min = alloc_consts.slab.large_min_blocks;
		}

		if(count < effective_min)
		{
			count = 0;
		}

		if(!count)
		{
			continue;
		}

		handle->slab_mask = ~(slab_size - 1);
		handle->slab_shift = MACRO_FLOOR_LOG2(slab_size);
		handle->padding = padding;
		handle->alloc_limit = count;
		handle->empty_slabs = 0;
		handle->slab_class = slab_class;
		handle->alloc_size = physical_size;
		handle->alignment = alignment;
		handle->slab = NULL;
		atomic_store_rx(&handle->foreign_slab, NULL);
		handle->next_page = NULL;

		return true;
	}

	return false;
}


void
alloc_default_handle_templates_rebuild(
	void
	)
{
	memset(alloc_consts.handles.templates, 0, sizeof(alloc_consts.handles.templates));

	alloc_t min_k = MACRO_FLOOR_LOG2(sizeof(void*));
	alloc_t max_k = MACRO_FLOOR_LOG2(alloc_consts.slab.virtual_cutoff);
	hard_assert_lt(max_k - min_k, ALLOC_MAX_HANDLE_COUNT);

	alloc_t success_idx = (alloc_t) -1;
	for(alloc_t k = min_k; k <= max_k; ++k)
	{
		alloc_t idx = k - min_k;
		alloc_handle_info_t info =
		(alloc_handle_info_t)
		{
			.alloc_size = (alloc_t) 1 << k,
			.alignment = (alloc_t) 1 << k
		};

		if(!alloc_create_handle(&info, alloc_consts.handles.templates + idx))
		{
			break;
		}

		success_idx = idx;
	}

	hard_assert_neq(success_idx, (alloc_t) -1);
	alloc_consts.handles.count = success_idx + 1;
	alloc_consts.slab.max_nonvirtual_size = (alloc_t) 1 << (success_idx + min_k);
}


void
alloc_rebuild_derived_geometry(
	void
	)
{
	alloc_consts.arena.slab_count = alloc_consts.arena.size >> alloc_consts.slab.small_shift;
	hard_assert_le(alloc_consts.arena.slab_count, 64);

	alloc_t slab_ptrs = sizeof(alloc_arena_header_t*) * alloc_consts.slab.class_count;
	alloc_consts.arena.btree_offset = MACRO_ALIGN_UP(sizeof(alloc_arena_header_t), alignof(uint64_t) - 1);
	alloc_consts.arena.vacant_next_offset = MACRO_ALIGN_UP(alloc_consts.arena.btree_offset + alloc_btree_size(), alignof(alloc_arena_header_t*) - 1);
	alloc_consts.arena.vacant_prev_offset = alloc_consts.arena.vacant_next_offset + slab_ptrs;
	alloc_consts.arena.slab_indexable_offset = MACRO_ALIGN_UP(alloc_consts.arena.vacant_prev_offset + slab_ptrs, alignof(alloc_slab_header_t) - 1);
	alloc_consts.arena.prefix_size = MACRO_ALIGN_UP(alloc_consts.arena.slab_indexable_offset
		+ sizeof(alloc_slab_header_t) * alloc_consts.arena.slab_count, alignof(alloc_slab_header_t) - 1);
	alloc_consts.arena.data_region_size = alloc_consts.arena.size * alloc_consts.arena.virtual_capacity;
	hard_assert_true(MACRO_IS_POWER_OF_2(alloc_consts.arena.data_region_size));
	alloc_consts.arena.data_region_mask = alloc_consts.arena.data_region_size - 1;
	alloc_consts.arena.prefix_region_offset = alloc_consts.arena.data_region_size;
	alloc_consts.arena.data_offset = 0;
	alloc_consts.arena.prefix_slabs = 0;

	memset(alloc_consts.arena.btree_template, 0, sizeof(alloc_consts.arena.btree_template));
	alloc_btree_init(alloc_consts.arena.btree_template);
	alloc_consts.arena.btree_avail_mask_template = 0;

	for(alloc_t class = 0; class < alloc_consts.slab.class_count; ++class)
	{
		if(alloc_consts.arena.btree_template[class])
		{
			alloc_consts.arena.btree_avail_mask_template |= (uint32_t) 1 << class;
		}
	}

	alloc_default_handle_templates_rebuild();
}


void
alloc_calculate_slab_geometry(
	void
	)
{
	alloc_apply_slab_geometry();
	alloc_rebuild_derived_geometry();
}


void
alloc_log_consts_dump(
	const char* cause
	)
{
	if(!alloc_consts.log.dump_consts)
	{
		return;
	}

	alloc_do_log_info(
		"consts_dump(): cause=", cause,
		"\n{"
		"\n\t.red_zone ="
		"\n\t{"
		"\n\t\t.size = ", alloc_consts.red_zone.size, ","
		"\n\t\t.random = ", alloc_consts.red_zone.random,
		"\n\t},"
		"\n\t.log ="
		"\n\t{"
		"\n\t\t.verbosity = ", alloc_consts.log.verbosity, ","
		"\n\t\t.fd = ", alloc_consts.log.fd, ","
		"\n\t\t.dump_consts = ", alloc_consts.log.dump_consts,
		"\n\t},"
		"\n\t.report ="
		"\n\t{"
		"\n\t\t.fd = ", alloc_consts.report.fd, ","
		"\n\t\t.period_ms = ", alloc_consts.report.period_ms, ","
		"\n\t\t.enable = ", alloc_consts.report.enable, ","
		"\n\t\t.per_thread_enable = ", alloc_consts.report.per_thread_enable,
		"\n\t},"
		"\n\t.bootstrap ="
		"\n\t{"
		"\n\t\t.size = ", alloc_consts.bootstrap.size, ","
		"\n\t\t.reserve_size = ", alloc_consts.bootstrap.reserve_size,
		"\n\t},"
		"\n\t.huge ="
		"\n\t{"
		"\n\t\t.calibrate_ms = ", alloc_consts.huge.calibrate_ms, ","
		"\n\t\t.threshold_ms = ", alloc_consts.huge.threshold_ms, ","
		"\n\t\t.slots = ", alloc_consts.huge.slots, ","
		"\n\t\t.slots_shift = ", alloc_consts.huge.slots_shift,
		"\n\t},"
		"\n\t.page ="
		"\n\t{"
		"\n\t\t.size = ", alloc_consts.page.size, ","
		"\n\t\t.shift = ", alloc_consts.page.shift,
		"\n\t},"
		"\n\t.huge_page ="
		"\n\t{"
		"\n\t\t.size = ", alloc_consts.huge_page.size, ","
		"\n\t\t.shift = ", alloc_consts.huge_page.shift,
		"\n\t},"
		"\n\t.arena ="
		"\n\t{"
		"\n\t\t.size = ", alloc_consts.arena.size, ","
		"\n\t\t.shift = ", alloc_consts.arena.shift, ","
		"\n\t\t.virtual_capacity = ", alloc_consts.arena.virtual_capacity, ","
		"\n\t\t.max_free_per_thread = ", alloc_consts.arena.max_free_per_thread, ","
		"\n\t\t.preallocate = ", alloc_consts.arena.preallocate, ","
		"\n\t\t.commit_batch = ", alloc_consts.arena.commit_batch, ","
		"\n\t\t.thp_enable = ", alloc_consts.arena.thp_enable, ","
		"\n\t\t.align = ", alloc_consts.arena.align, ","
		"\n\t\t.tail_decommit_enable = ", alloc_consts.arena.tail_decommit_enable, ","
		"\n\t\t.tail_decommit_trigger_div = ", alloc_consts.arena.tail_decommit_trigger_div, ","
		"\n\t\t.tail_decommit_amount_div = ", alloc_consts.arena.tail_decommit_amount_div, ","
		"\n\t\t.prefix_thp_enable = ", alloc_consts.arena.prefix_thp_enable, ","
		"\n\t\t.prefix_tail_decommit_enable = ", alloc_consts.arena.prefix_tail_decommit_enable, ","
		"\n\t\t.prefix_tail_decommit_trigger_div = ", alloc_consts.arena.prefix_tail_decommit_trigger_div, ","
		"\n\t\t.prefix_tail_decommit_amount_div = ", alloc_consts.arena.prefix_tail_decommit_amount_div, ","
		"\n\t\t.btree_offset = ", alloc_consts.arena.btree_offset, ","
		"\n\t\t.vacant_next_offset = ", alloc_consts.arena.vacant_next_offset, ","
		"\n\t\t.vacant_prev_offset = ", alloc_consts.arena.vacant_prev_offset, ","
		"\n\t\t.slab_indexable_offset = ", alloc_consts.arena.slab_indexable_offset, ","
		"\n\t\t.prefix_size = ", alloc_consts.arena.prefix_size, ","
		"\n\t\t.data_region_size = ", alloc_consts.arena.data_region_size, ","
		"\n\t\t.prefix_region_offset = ", alloc_consts.arena.prefix_region_offset, ","
		"\n\t\t.data_offset = ", alloc_consts.arena.data_offset, ","
		"\n\t\t.slab_count = ", alloc_consts.arena.slab_count, ","
		"\n\t\t.prefix_slabs = ", alloc_consts.arena.prefix_slabs,
		"\n\t},"
		"\n\t.slab ="
		"\n\t{"
		"\n\t\t.small = ", alloc_consts.slab.small, ","
		"\n\t\t.small_shift = ", alloc_consts.slab.small_shift, ","
		"\n\t\t.medium = ", alloc_consts.slab.medium, ","
		"\n\t\t.medium_shift = ", alloc_consts.slab.medium_shift, ","
		"\n\t\t.large = ", alloc_consts.slab.large, ","
		"\n\t\t.large_shift = ", alloc_consts.slab.large_shift, ","
		"\n\t\t.class_count = ", alloc_consts.slab.class_count, ","
		"\n\t\t.small_min_blocks = ", alloc_consts.slab.small_min_blocks, ","
		"\n\t\t.small_min_blocks_shift = ", alloc_consts.slab.small_min_blocks_shift, ","
		"\n\t\t.medium_min_blocks = ", alloc_consts.slab.medium_min_blocks, ","
		"\n\t\t.medium_min_blocks_shift = ", alloc_consts.slab.medium_min_blocks_shift, ","
		"\n\t\t.large_min_blocks = ", alloc_consts.slab.large_min_blocks, ","
		"\n\t\t.large_min_blocks_shift = ", alloc_consts.slab.large_min_blocks_shift, ","
		"\n\t\t.virtual_cutoff = ", alloc_consts.slab.virtual_cutoff, ","
		"\n\t\t.virtual_cutoff_shift = ", alloc_consts.slab.virtual_cutoff_shift, ","
		"\n\t\t.max_nonvirtual_size = ", alloc_consts.slab.max_nonvirtual_size, ","
		"\n\t\t.max_free_per_handle = ", alloc_consts.slab.max_free_per_handle, ","
		"\n\t\t.smallest_allocation_shift = ", alloc_consts.slab.smallest_allocation_shift,
		"\n\t},"
		"\n\t.handles ="
		"\n\t{"
		"\n\t\t.count = ", alloc_consts.handles.count,
		"\n\t},"
		"\n\t.tcache ="
		"\n\t{"
		"\n\t\t.max_per_class = ", alloc_consts.tcache.max_per_class, ","
		"\n\t\t.capacity = ", alloc_consts.tcache.capacity,
		"\n\t},"
		"\n\t.tcb ="
		"\n\t{"
		"\n\t\t.capacity = ", alloc_consts.tcb.capacity, ","
		"\n\t\t.preallocate = ", alloc_consts.tcb.preallocate, ","
		"\n\t\t.commit_batch = ", alloc_consts.tcb.commit_batch,
		"\n\t},"
		"\n\t.numa ="
		"\n\t{"
		"\n\t\t.arena_bitmap_l0_size = ", alloc_consts.numa.arena_bitmap_l0_size, ","
		"\n\t\t.arena_bitmap_l0_offset = ", alloc_consts.numa.arena_bitmap_l0_offset, ","
		"\n\t\t.arena_bitmap_l1_size = ", alloc_consts.numa.arena_bitmap_l1_size, ","
		"\n\t\t.arena_bitmap_l1_offset = ", alloc_consts.numa.arena_bitmap_l1_offset, ","
		"\n\t\t.huge_nodes_offset = ", alloc_consts.numa.huge_nodes_offset, ","
		"\n\t\t.huge_ht_offset = ", alloc_consts.numa.huge_ht_offset, ","
		"\n\t\t.tcb_size = ", alloc_consts.numa.tcb_size, ","
		"\n\t\t.tcb_preallocate_size = ", alloc_consts.numa.tcb_preallocate_size, ","
		"\n\t\t.tcb_offset = ", alloc_consts.numa.tcb_offset, ","
		"\n\t\t.data_size = ", alloc_consts.numa.data_size, ","
		"\n\t\t.preallocated_size = ", alloc_consts.numa.preallocated_size, ","
		"\n\t\t.nodes = ", alloc_consts.numa.nodes, ","
		"\n\t\t.valid = ", alloc_consts.numa.valid,
		"\n\t}"
		"\n}");
}


void
alloc_init(
	void
	)
{
	alloc_consts = (alloc_consts_t){0};

	alloc_env_t env = alloc_parse_env();
	alloc_validate_env(&env);

#define CHOOSE(name, ...)	\
	env.name##_set ? env.name : (__VA_ARGS__)

	alloc_consts.red_zone.size = CHOOSE(red_zone_size, alloc_defaults.red_zone.size);
	alloc_consts.red_zone.random = alloc_get_random();

	alloc_consts.log.verbosity = CHOOSE(log_verbosity, alloc_defaults.log.verbosity);
	alloc_consts.log.fd = CHOOSE(log_fd, alloc_defaults.log.fd);
	alloc_consts.log.dump_consts = CHOOSE(log_dump_consts, alloc_defaults.log.dump_consts);

	alloc_consts.report.fd = CHOOSE(report_fd, alloc_defaults.report.fd);
	alloc_consts.report.period_ms = CHOOSE(report_period_ms, alloc_defaults.report.period_ms);
	alloc_consts.report.enable = CHOOSE(report_enable, alloc_defaults.report.enable);
	alloc_consts.report.per_thread_enable = CHOOSE(report_per_thread_enable, alloc_defaults.report.per_thread_enable);
	alloc_report_init();

	alloc_consts.bootstrap.size = CHOOSE(bootstrap_size, alloc_defaults.bootstrap.size);
	alloc_t reserve_size = CHOOSE(bootstrap_reserve_size, alloc_defaults.bootstrap.reserve_size);
	alloc_consts.bootstrap.reserve_size = MACRO_MIN(reserve_size, alloc_consts.bootstrap.size / 2);
	alloc_bootstrap_init();

	alloc_consts.huge.calibrate_ms = CHOOSE(huge_calibrate_ms, alloc_defaults.huge.calibrate_ms);
	alloc_consts.huge.calibrate_ns = alloc_consts.huge.calibrate_ms * 1000000;
	alloc_consts.huge.threshold_ms = CHOOSE(huge_threshold_ms, alloc_defaults.huge.threshold_ms);
	alloc_consts.huge.threshold_ns = alloc_consts.huge.threshold_ms * 1000000;
	alloc_consts.huge.slots = CHOOSE(huge_slots, alloc_defaults.huge.slots);
	alloc_consts.huge.slots_mask = alloc_consts.huge.slots - 1;
	alloc_consts.huge.slots_shift = MACRO_FLOOR_LOG2(alloc_consts.huge.slots);
	alloc_huge_global_init();

	alloc_consts.page.size = alloc_read_page_size();
	hard_assert_neq(alloc_consts.page.size, 0);
	hard_assert_true(MACRO_IS_POWER_OF_2(alloc_consts.page.size));
	alloc_consts.page.mask = alloc_consts.page.size - 1;
	alloc_consts.page.shift = MACRO_FLOOR_LOG2(alloc_consts.page.size);

	alloc_consts.huge_page.size = alloc_read_huge_page_size();
	if(alloc_consts.huge_page.size)
	{
		hard_assert_true(MACRO_IS_POWER_OF_2(alloc_consts.huge_page.size));
		alloc_consts.huge_page.mask = alloc_consts.huge_page.size - 1;
		alloc_consts.huge_page.shift = MACRO_FLOOR_LOG2(alloc_consts.huge_page.size);
	}

	atomic_store_rel(&alloc_consts.state, ALLOC_CONSTS_STATE_BOOTSTRAP);

	alloc_log_unknown(&env);
	alloc_log_ignored(&env);
	alloc_log_rounded(&env);

	alloc_consts.arena.size = CHOOSE(arena_size, alloc_defaults.arena.size);
	alloc_consts.arena.mask = alloc_consts.arena.size - 1;
	alloc_consts.arena.shift = MACRO_FLOOR_LOG2(alloc_consts.arena.size);

	alloc_consts.arena.virtual_capacity = CHOOSE(arena_virtual_capacity, alloc_defaults.arena.virtual_capacity);
	hard_assert_true(MACRO_IS_POWER_OF_2(alloc_consts.arena.virtual_capacity));
	alloc_consts.arena.max_free_per_thread = CHOOSE(arena_max_free_per_thread, alloc_defaults.arena.max_free_per_thread);
	alloc_consts.arena.preallocate = CHOOSE(arena_preallocate, alloc_defaults.arena.preallocate);
	hard_assert_le(alloc_consts.arena.preallocate, alloc_consts.arena.virtual_capacity);
	alloc_consts.arena.commit_batch = CHOOSE(arena_commit_batch, alloc_defaults.arena.commit_batch);
	hard_assert_le(alloc_consts.arena.commit_batch, alloc_consts.arena.virtual_capacity);
	alloc_consts.arena.thp_enable = CHOOSE(arena_thp_enable, alloc_defaults.arena.thp_enable);
	alloc_t thp_align = MACRO_MAX(alloc_consts.huge_page.size, alloc_consts.arena.size);
	alloc_consts.arena.align = alloc_consts.arena.thp_enable ? thp_align : alloc_consts.arena.size;
	alloc_consts.arena.tail_decommit_enable = CHOOSE(arena_tail_decommit_enable, alloc_defaults.arena.tail_decommit_enable);
	alloc_consts.arena.tail_decommit_trigger_div = CHOOSE(arena_tail_decommit_trigger_div, alloc_defaults.arena.tail_decommit_trigger_div);
	alloc_consts.arena.tail_decommit_amount_div = CHOOSE(arena_tail_decommit_amount_div, alloc_defaults.arena.tail_decommit_amount_div);
	alloc_consts.arena.prefix_thp_enable = CHOOSE(arena_prefix_thp_enable, alloc_defaults.arena.prefix_thp_enable);
	alloc_consts.arena.prefix_tail_decommit_enable = CHOOSE(arena_prefix_tail_decommit_enable, alloc_defaults.arena.prefix_tail_decommit_enable);
	alloc_consts.arena.prefix_tail_decommit_trigger_div = CHOOSE(arena_prefix_tail_decommit_trigger_div, alloc_defaults.arena.prefix_tail_decommit_trigger_div);
	alloc_consts.arena.prefix_tail_decommit_amount_div = CHOOSE(arena_prefix_tail_decommit_amount_div, alloc_defaults.arena.prefix_tail_decommit_amount_div);

	alloc_consts.slab.small = MACRO_MAX(alloc_consts.page.size, CHOOSE(slab_small, alloc_defaults.slab.small));
	alloc_consts.slab.small_shift = MACRO_FLOOR_LOG2(alloc_consts.slab.small);

	alloc_consts.slab.medium = MACRO_MAX(alloc_consts.slab.small, CHOOSE(slab_medium, alloc_defaults.slab.medium));
	hard_assert_gt(alloc_consts.slab.medium, alloc_consts.slab.small);
	alloc_consts.slab.medium_shift = MACRO_FLOOR_LOG2(alloc_consts.slab.medium);

	alloc_consts.slab.large = MACRO_MAX(alloc_consts.slab.medium, CHOOSE(slab_large, alloc_defaults.slab.large));
	hard_assert_gt(alloc_consts.slab.large, alloc_consts.slab.medium);
	alloc_consts.slab.large_shift = MACRO_FLOOR_LOG2(alloc_consts.slab.large);

	alloc_consts.slab.small_min_blocks = CHOOSE(slab_small_min_blocks, alloc_defaults.slab.small_min_blocks);
	hard_assert_le(alloc_consts.slab.small_min_blocks, alloc_consts.slab.small / sizeof(void*));
	alloc_consts.slab.small_min_blocks_shift = MACRO_FLOOR_LOG2(alloc_consts.slab.small_min_blocks);
	alloc_consts.slab.medium_min_blocks = CHOOSE(slab_medium_min_blocks, alloc_defaults.slab.medium_min_blocks);
	hard_assert_le(alloc_consts.slab.medium_min_blocks, alloc_consts.slab.medium / sizeof(void*));
	alloc_consts.slab.medium_min_blocks_shift = MACRO_FLOOR_LOG2(alloc_consts.slab.medium_min_blocks);
	alloc_consts.slab.large_min_blocks = CHOOSE(slab_large_min_blocks, alloc_defaults.slab.large_min_blocks);
	hard_assert_le(alloc_consts.slab.large_min_blocks, alloc_consts.slab.large / sizeof(void*));
	alloc_consts.slab.large_min_blocks_shift = MACRO_FLOOR_LOG2(alloc_consts.slab.large_min_blocks);

	alloc_consts.slab.virtual_cutoff = alloc_consts.slab.large >> alloc_consts.slab.large_min_blocks_shift;
	alloc_consts.slab.virtual_cutoff_shift = MACRO_FLOOR_LOG2(alloc_consts.slab.virtual_cutoff);
	alloc_consts.slab.max_free_per_handle = CHOOSE(slab_max_free_per_handle, alloc_defaults.slab.max_free_per_handle);
	alloc_consts.slab.smallest_allocation_shift = MACRO_FLOOR_LOG2(sizeof(void*));

	alloc_calculate_slab_geometry();

	alloc_consts.tcache.max_per_class = CHOOSE(tcache_max_per_class, alloc_defaults.tcache.max_per_class);
	hard_assert_true(!alloc_consts.tcache.max_per_class || MACRO_IS_POWER_OF_2(alloc_consts.tcache.max_per_class));
	alloc_consts.tcache.capacity = alloc_consts.tcache.max_per_class << 1;
	alloc_consts.tcache.mask = alloc_consts.tcache.capacity ? alloc_consts.tcache.capacity - 1 : 0;
	hard_assert_le(alloc_consts.tcache.capacity, ALLOC_MAX_TCACHE_STACK_CAPACITY);

	alloc_consts.tcb.capacity = CHOOSE(tcb_capacity, alloc_defaults.tcb.capacity);
	alloc_consts.tcb.preallocate = CHOOSE(tcb_preallocate, alloc_defaults.tcb.preallocate);
	hard_assert_le(alloc_consts.tcb.preallocate, alloc_consts.tcb.capacity);
	alloc_consts.tcb.commit_batch = CHOOSE(tcb_commit_batch, alloc_defaults.tcb.commit_batch);
	hard_assert_le(alloc_consts.tcb.commit_batch, alloc_consts.tcb.capacity);

	alloc_consts.numa.arena_bitmap_l0_size = (alloc_consts.arena.virtual_capacity + 63) >> 6;
	alloc_consts.numa.arena_bitmap_l0_offset = sizeof(alloc_numa_local_data_t);
	alloc_consts.numa.arena_bitmap_l1_size = (alloc_consts.numa.arena_bitmap_l0_size + 63) >> 6;
	alloc_consts.numa.arena_bitmap_l1_offset = alloc_consts.numa.arena_bitmap_l0_offset +
		sizeof(uint64_t) * (alloc_consts.numa.arena_bitmap_l0_size + 1);
	alloc_consts.numa.huge_nodes_offset = alloc_consts.numa.arena_bitmap_l1_offset +
		sizeof(uint64_t) * (alloc_consts.numa.arena_bitmap_l1_size + 1);
	alloc_consts.numa.huge_ht_offset = alloc_consts.numa.huge_nodes_offset +
		sizeof(alloc_huge_node_t) * alloc_consts.huge.slots;
	alloc_consts.numa.tcb_size = sizeof(alloc_tcb_t) * alloc_consts.tcb.capacity;
	alloc_consts.numa.tcb_preallocate_size = sizeof(alloc_tcb_t) * alloc_consts.tcb.preallocate;
	alloc_consts.numa.tcb_offset = alloc_consts.numa.huge_ht_offset + sizeof(uint64_t) * alloc_consts.huge.slots;

	alloc_consts.numa.nodes = alloc_get_numa_nodes();
	hard_assert_le(alloc_consts.numa.nodes, ALLOC_MAX_NUMA_NODES);
	alloc_consts.numa.data_size = alloc_consts.numa.tcb_offset + alloc_consts.numa.tcb_size;
	alloc_consts.numa.preallocated_size = alloc_consts.numa.tcb_offset + alloc_consts.numa.tcb_preallocate_size;

	if(alloc_consts.numa.nodes == 1)
	{
		alloc_consts.numa.valid = 1;

		alloc_numa_local_data_t* data = alloc_alloc_virtual_e(
			alloc_consts.numa.data_size, alloc_consts.numa.preallocated_size, -1, 0);
		hard_assert_not_null(data);

		data->arena.bitmap_l0 = (void*) data + alloc_consts.numa.arena_bitmap_l0_offset;
		data->arena.bitmap_l1 = (void*) data + alloc_consts.numa.arena_bitmap_l1_offset;
		data->huge.ht = (void*) data + alloc_consts.numa.huge_ht_offset;
		data->huge.nodes = (void*) data + alloc_consts.numa.huge_nodes_offset;
		data->tcb.ptr = (void*) data + alloc_consts.numa.tcb_offset;

		alloc_consts.numa.data.single = data;
		sync_mtx_init(&data->arena.mtx);
		alloc_init_arena_pool(data, 0);

		sync_mtx_init(&data->tcb.mtx);
		data->tcb.committed = alloc_consts.tcb.preallocate;

		alloc_huge_init(data);
	}
	else
	{
		for(uint16_t numa = 0; numa < alloc_consts.numa.nodes; ++numa)
		{
			if(alloc_is_numa_node_valid(numa))
			{
				alloc_consts.numa.map[numa] = alloc_consts.numa.valid++;
				alloc_consts.numa.reverse_map[alloc_consts.numa.map[numa]] = numa;
			}
		}

		alloc_consts.numa.data.multiple = alloc_alloc_virtual(
			alloc_consts.numa.data.multiple, alloc_consts.numa.valid, -1, -1, 0);
		hard_assert_not_null(alloc_consts.numa.data.multiple);

		alloc_numa_local_data_t** data_ptr = alloc_consts.numa.data.multiple;
		alloc_numa_local_data_t** data_ptr_end = data_ptr + alloc_consts.numa.valid;

		alloc_numa_t numa = 0;

		while(data_ptr < data_ptr_end)
		{
			alloc_numa_local_data_t* data = alloc_alloc_virtual_e(alloc_consts.numa.data_size,
				alloc_consts.numa.preallocated_size, alloc_consts.numa.reverse_map[numa], 0);
			assert_not_null(data);

			data->arena.bitmap_l0 = (void*) data + alloc_consts.numa.arena_bitmap_l0_offset;
			data->arena.bitmap_l1 = (void*) data + alloc_consts.numa.arena_bitmap_l1_offset;
			data->huge.ht = (void*) data + alloc_consts.numa.huge_ht_offset;
			data->huge.nodes = (void*) data + alloc_consts.numa.huge_nodes_offset;
			data->tcb.ptr = (void*) data + alloc_consts.numa.tcb_offset;

			*data_ptr = data;
			sync_mtx_init(&data->arena.mtx);
			alloc_init_arena_pool(data, alloc_consts.numa.reverse_map[numa]);

			sync_mtx_init(&data->tcb.mtx);
			data->tcb.committed = alloc_consts.tcb.preallocate;

			alloc_huge_init(data);

			++data_ptr;
			++numa;
		}
	}

	alloc_consts.thread.key = thread_key_create(alloc_thread_dtor_fn);

#undef CHOOSE

	atomic_store_rel(&alloc_consts.state, ALLOC_CONSTS_STATE_INITIALIZED);

	alloc_log_consts_dump("init");
	alloc_report_refresh();
}


attr_ctor void
alloc_consts_init(
	void
	)
{
	alloc_init();
}


attr_dtor void
alloc_consts_free(
	void
	)
{
	alloc_report_stop();
	alloc_thread_dtor_fn(NULL);
}


void
alloc_configure(
	const alloc_config_t* config
	)
{
	assert_not_null(config);

#ifndef ALLOC_RELEASE
	assert_false(atomic_load_acq(&alloc_consts.first_use_done),
		alloc_log_error("configure(): cannot be called after the first alloc/free"));
#endif

#define ALLOC_CONFIG_VALIDATE(name, cond)		\
	if(config->name##_set && !(cond))			\
	{											\
		alloc_log_error(						\
			"configure(): invalid value for "	\
			#name "=", config->name);			\
	}											\
	else if(config->name##_set)

#ifndef ALLOC_RELEASE
	ALLOC_CONFIG_VALIDATE(red_zone_size,
		config->red_zone_size <= (alloc_t) 1 << 16
		)
	{
		alloc_consts.red_zone.size = config->red_zone_size;
	}
#else
	if(config->red_zone_size_set && config->red_zone_size)
	{
		alloc_log_warning("configure(): red zones are disabled in release builds, ignoring red_zone_size=", config->red_zone_size);
	}
#endif

	ALLOC_CONFIG_VALIDATE(log_verbosity,
		config->log_verbosity < ALLOC_LOG_VERBOSITY__COUNT
		)
	{
		alloc_consts.log.verbosity = config->log_verbosity;
	}

	ALLOC_CONFIG_VALIDATE(log_fd,
		config->log_fd <= INT32_MAX
		)
	{
		alloc_consts.log.fd = config->log_fd;
	}

	ALLOC_CONFIG_VALIDATE(log_dump_consts,
		config->log_dump_consts <= 1
		)
	{
		alloc_consts.log.dump_consts = config->log_dump_consts;
	}

	ALLOC_CONFIG_VALIDATE(report_fd,
		config->report_fd <= INT32_MAX
		)
	{
		alloc_consts.report.fd = config->report_fd;
	}

	ALLOC_CONFIG_VALIDATE(report_period_ms,
		config->report_period_ms &&
		config->report_period_ms <= 86400000
		)
	{
		alloc_consts.report.period_ms = config->report_period_ms;
	}

	ALLOC_CONFIG_VALIDATE(report_enable,
		config->report_enable <= 1
		)
	{
		alloc_consts.report.enable = config->report_enable;
	}

	ALLOC_CONFIG_VALIDATE(report_per_thread_enable,
		config->report_per_thread_enable <= 1
		)
	{
		alloc_consts.report.per_thread_enable = config->report_per_thread_enable;
	}

	ALLOC_CONFIG_VALIDATE(huge_threshold_ms,
		config->huge_threshold_ms &&
		config->huge_threshold_ms <= 86400000
		)
	{
		alloc_consts.huge.threshold_ms = config->huge_threshold_ms;
		alloc_consts.huge.threshold_ns = alloc_consts.huge.threshold_ms * 1000000;
	}

	ALLOC_CONFIG_VALIDATE(arena_max_free_per_thread,
		config->arena_max_free_per_thread &&
		config->arena_max_free_per_thread <= ALLOC_MAX_ARENA_FREE_PER_THREAD
		)
	{
		alloc_consts.arena.max_free_per_thread = config->arena_max_free_per_thread;
	}

	ALLOC_CONFIG_VALIDATE(arena_commit_batch,
		config->arena_commit_batch &&
		config->arena_commit_batch <= (alloc_t) 1 << 16
		)
	{
		alloc_consts.arena.commit_batch = config->arena_commit_batch;
	}

	ALLOC_CONFIG_VALIDATE(arena_tail_decommit_enable,
		config->arena_tail_decommit_enable <= 1
		)
	{
		alloc_consts.arena.tail_decommit_enable = config->arena_tail_decommit_enable;
	}

	ALLOC_CONFIG_VALIDATE(arena_tail_decommit_trigger_div,
		config->arena_tail_decommit_trigger_div >= 2 &&
		config->arena_tail_decommit_trigger_div <= 64
		)
	{
		alloc_consts.arena.tail_decommit_trigger_div = config->arena_tail_decommit_trigger_div;
	}

	ALLOC_CONFIG_VALIDATE(arena_tail_decommit_amount_div,
		config->arena_tail_decommit_amount_div >= 2 &&
		config->arena_tail_decommit_amount_div <= 64
		)
	{
		alloc_consts.arena.tail_decommit_amount_div = config->arena_tail_decommit_amount_div;
	}

	ALLOC_CONFIG_VALIDATE(arena_prefix_tail_decommit_enable,
		config->arena_prefix_tail_decommit_enable <= 1
		)
	{
		alloc_consts.arena.prefix_tail_decommit_enable = config->arena_prefix_tail_decommit_enable;
	}

	ALLOC_CONFIG_VALIDATE(arena_prefix_tail_decommit_trigger_div,
		config->arena_prefix_tail_decommit_trigger_div >= 2 &&
		config->arena_prefix_tail_decommit_trigger_div <= 64
		)
	{
		alloc_consts.arena.prefix_tail_decommit_trigger_div = config->arena_prefix_tail_decommit_trigger_div;
	}

	ALLOC_CONFIG_VALIDATE(arena_prefix_tail_decommit_amount_div,
		config->arena_prefix_tail_decommit_amount_div >= 2 &&
		config->arena_prefix_tail_decommit_amount_div <= 64
		)
	{
		alloc_consts.arena.prefix_tail_decommit_amount_div = config->arena_prefix_tail_decommit_amount_div;
	}

	ALLOC_CONFIG_VALIDATE(slab_small,
		config->slab_small &&
		config->slab_small <= alloc_consts.arena.size &&
		MACRO_IS_POWER_OF_2(config->slab_small)
		)
	{
		alloc_consts.slab.small = config->slab_small;
		alloc_consts.slab.small_shift = MACRO_FLOOR_LOG2(alloc_consts.slab.small);
	}

	ALLOC_CONFIG_VALIDATE(slab_medium,
		config->slab_medium >= alloc_consts.slab.small &&
		config->slab_medium <= alloc_consts.arena.size &&
		MACRO_IS_POWER_OF_2(config->slab_medium)
		)
	{
		alloc_consts.slab.medium = config->slab_medium;
		alloc_consts.slab.medium_shift = MACRO_FLOOR_LOG2(alloc_consts.slab.medium);
	}

	ALLOC_CONFIG_VALIDATE(slab_large,
		config->slab_large >= alloc_consts.slab.medium &&
		config->slab_large <= alloc_consts.arena.size &&
		MACRO_IS_POWER_OF_2(config->slab_large)
		)
	{
		alloc_consts.slab.large = config->slab_large;
		alloc_consts.slab.large_shift = MACRO_FLOOR_LOG2(alloc_consts.slab.large);
	}

	ALLOC_CONFIG_VALIDATE(slab_small_min_blocks,
		config->slab_small_min_blocks &&
		config->slab_small_min_blocks <= alloc_consts.slab.small / sizeof(void*) &&
		MACRO_IS_POWER_OF_2(config->slab_small_min_blocks)
		)
	{
		alloc_consts.slab.small_min_blocks = config->slab_small_min_blocks;
		alloc_consts.slab.small_min_blocks_shift = MACRO_FLOOR_LOG2(alloc_consts.slab.small_min_blocks);
	}

	ALLOC_CONFIG_VALIDATE(slab_medium_min_blocks,
		config->slab_medium_min_blocks &&
		config->slab_medium_min_blocks <= alloc_consts.slab.medium / sizeof(void*) &&
		MACRO_IS_POWER_OF_2(config->slab_medium_min_blocks)
		)
	{
		alloc_consts.slab.medium_min_blocks = config->slab_medium_min_blocks;
		alloc_consts.slab.medium_min_blocks_shift = MACRO_FLOOR_LOG2(alloc_consts.slab.medium_min_blocks);
	}

	ALLOC_CONFIG_VALIDATE(slab_large_min_blocks,
		config->slab_large_min_blocks &&
		config->slab_large_min_blocks <= alloc_consts.slab.large / sizeof(void*) &&
		MACRO_IS_POWER_OF_2(config->slab_large_min_blocks)
		)
	{
		alloc_consts.slab.large_min_blocks = config->slab_large_min_blocks;
		alloc_consts.slab.large_min_blocks_shift = MACRO_FLOOR_LOG2(alloc_consts.slab.large_min_blocks);
	}

	ALLOC_CONFIG_VALIDATE(slab_max_free_per_handle,
		config->slab_max_free_per_handle <= (alloc_t) 1 << 16
		)
	{
		alloc_consts.slab.max_free_per_handle = config->slab_max_free_per_handle;
	}

	alloc_consts.slab.virtual_cutoff = alloc_consts.slab.large >> alloc_consts.slab.large_min_blocks_shift;
	alloc_consts.slab.virtual_cutoff_shift = MACRO_FLOOR_LOG2(alloc_consts.slab.virtual_cutoff);

	alloc_calculate_slab_geometry();

	ALLOC_CONFIG_VALIDATE(tcache_max_per_class,
		config->tcache_max_per_class <= ALLOC_MAX_TCACHE_CAPACITY &&
		(!config->tcache_max_per_class || MACRO_IS_POWER_OF_2(config->tcache_max_per_class))
		)
	{
		alloc_consts.tcache.max_per_class = config->tcache_max_per_class;
		alloc_consts.tcache.capacity = alloc_consts.tcache.max_per_class << 1;
		alloc_consts.tcache.mask = alloc_consts.tcache.capacity ? alloc_consts.tcache.capacity - 1 : 0;
	}

	ALLOC_CONFIG_VALIDATE(tcb_commit_batch,
		config->tcb_commit_batch &&
		config->tcb_commit_batch <= alloc_consts.tcb.capacity
		)
	{
		alloc_consts.tcb.commit_batch = config->tcb_commit_batch;
	}

	alloc_log_consts_dump("configure");
	alloc_report_refresh();

#undef ALLOC_CONFIG_VALIDATE
}


alloc_numa_local_data_t*
alloc_get_numa_local_data(
	alloc_numa_t numa
	)
{
	if(alloc_consts.numa.nodes == 1)
	{
		return alloc_consts.numa.data.single;
	}

	assert_lt(numa, alloc_consts.numa.valid);

	return alloc_consts.numa.data.multiple[numa];
}


alloc_numa_t
alloc_numa_to_logical(
	uint16_t numa
	)
{
	if(alloc_consts.numa.nodes == 1)
	{
		return 0;
	}

	assert_lt(numa, alloc_consts.numa.nodes);

	return alloc_consts.numa.map[numa];
}


uint16_t
alloc_numa_to_physical(
	alloc_numa_t numa
	)
{
	if(alloc_consts.numa.nodes == 1)
	{
		return 0;
	}

	assert_lt(numa, alloc_consts.numa.valid);

	return alloc_consts.numa.reverse_map[numa];
}

/*
 *   Copyright 2024-2025 Franciszek Balcerak
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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "macro.h"

#include <stdint.h>
#include <stddef.h>

#ifndef _const_func_
	#define _const_func_ __attribute__((const))
#endif

#ifndef _pure_func_
	#define _pure_func_ __attribute__((pure))
#endif

#ifndef _warn_unused_result_
	#define _warn_unused_result_ __attribute__((warn_unused_result))
#endif

#ifndef _alloc_func_
	#define _alloc_func_ __attribute__((malloc)) _warn_unused_result_
#endif

#ifndef _nonnull_
	#define _nonnull_
#endif

#ifndef _inline_
	#define _inline_ __attribute__((always_inline)) inline
#endif

#ifndef _in_
	#define _in_ const
#endif

#ifndef _in_opt_
	#define _in_opt_ const
#endif

#ifndef _out_
	#define _out_ _nonnull_
#endif

#ifndef _out_opt_
	#define _out_opt_
#endif

#ifndef _inout_
	#define _inout_ _nonnull_
#endif

#ifndef _inout_opt_
	#define _inout_opt_
#endif

#ifndef _opaque_
	#define _opaque_ const
#endif


typedef uintptr_t alloc_t;


typedef enum alloc_handle_flag : alloc_t
{
	ALLOC_HANDLE_FLAG_NONE					= 0,
	ALLOC_HANDLE_FLAG_IMMEDIATE_FREE		= 1 << 0,
	ALLOC_HANDLE_FLAG_DO_NOT_FREE			= 1 << 1,
	ALLOC_HANDLE_FLAG_THREAD_LOCAL			= 1 << 2,
	MACRO_ENUM_BITS(ALLOC_HANDLE_FLAG)
}
alloc_handle_flag_t;


typedef struct alloc_handle
{
	alloc_t _[10 + 22];
}
alloc_handle_t;


typedef struct alloc_handle_info
{
	alloc_t alloc_size;
	alloc_t block_size;
	alloc_t alignment;
}
alloc_handle_info_t;


typedef uint32_t
(*alloc_idx_fn_t)(
	alloc_t size
	);


typedef struct alloc_state_info
{
	alloc_handle_info_t* handles;
	alloc_t handle_count;
	alloc_idx_fn_t idx_fn;
}
alloc_state_info_t;


typedef struct alloc_state
{
	alloc_idx_fn_t idx_fn;

	alloc_t handle_count;
	alloc_handle_t handles[];
}
alloc_state_t;


extern _const_func_ const alloc_state_t*
alloc_get_global_state(
	void
	);


extern _const_func_ alloc_t
alloc_get_page_size(
	void
	);


extern _const_func_ alloc_t
alloc_get_default_block_size(
	void
	);


extern _alloc_func_ void*
alloc_alloc_virtual(
	alloc_t size
	);


extern void
alloc_free_virtual(
	_opaque_ void* ptr,
	alloc_t size
	);


extern _alloc_func_ void*
alloc_alloc_virtual_aligned(
	alloc_t size,
	alloc_t alignment,
	_out_ void** ptr
	);


extern void
alloc_free_virtual_aligned(
	_opaque_ void* real_ptr,
	alloc_t size,
	alloc_t alignment
	);


extern _alloc_func_ void*
alloc_realloc_virtual(
	_opaque_ void* ptr,
	alloc_t old_size,
	alloc_t new_size
	);


extern _alloc_func_ void*
alloc_realloc_virtual_aligned(
	_opaque_ void* real_ptr,
	alloc_t old_size,
	alloc_t new_size,
	alloc_t alignment,
	_out_ void** new_ptr
	);


extern void
alloc_create_handle(
	_in_ alloc_handle_info_t* info,
	_opaque_ alloc_handle_t* handle
	);


extern void
alloc_clone_handle(
	_opaque_ alloc_handle_t* source,
	_opaque_ alloc_handle_t* handle
	);


extern void
alloc_free_handle(
	_opaque_ alloc_handle_t* handle
	);


extern _alloc_func_ const alloc_state_t*
alloc_alloc_state(
	_in_ alloc_state_info_t* info
	);


extern _alloc_func_ const alloc_state_t*
alloc_clone_state(
	_in_ alloc_state_t* source
	);


extern void
alloc_free_state(
	_opaque_ alloc_state_t* state
	);


extern _pure_func_ _opaque_ alloc_handle_t*
alloc_get_handle_s(
	_in_ alloc_state_t* state,
	alloc_t size
	);


extern void
alloc_handle_lock_h(
	_opaque_ alloc_handle_t* handle
	);


extern void
alloc_handle_unlock_h(
	_opaque_ alloc_handle_t* handle
	);


extern void
alloc_handle_set_flags_h(
	_opaque_ alloc_handle_t* handle,
	alloc_handle_flag_t flags
	);


extern void
alloc_handle_set_flags_uh(
	_opaque_ alloc_handle_t* handle,
	alloc_handle_flag_t flags
	);


extern void
alloc_handle_add_flags_h(
	_opaque_ alloc_handle_t* handle,
	alloc_handle_flag_t flags
	);


extern void
alloc_handle_add_flags_uh(
	_opaque_ alloc_handle_t* handle,
	alloc_handle_flag_t flags
	);


extern void
alloc_handle_del_flags_h(
	_opaque_ alloc_handle_t* handle,
	alloc_handle_flag_t flags
	);


extern void
alloc_handle_del_flags_uh(
	_opaque_ alloc_handle_t* handle,
	alloc_handle_flag_t flags
	);


extern alloc_handle_flag_t
alloc_handle_get_flags_h(
	_opaque_ alloc_handle_t* handle
	);


extern alloc_handle_flag_t
alloc_handle_get_flags_uh(
	_opaque_ alloc_handle_t* handle
	);


extern _alloc_func_ void*
alloc_alloc_h(
	_opaque_ alloc_handle_t* handle,
	alloc_t size,
	int zero
	);


extern _alloc_func_ void*
alloc_alloc_uh(
	_opaque_ alloc_handle_t* handle,
	alloc_t size,
	int zero
	);


extern void
alloc_free_h(
	_opaque_ alloc_handle_t* handle,
	_opaque_ void* ptr,
	alloc_t size
	);


extern void
alloc_free_uh(
	_opaque_ alloc_handle_t* handle,
	_opaque_ void* ptr,
	alloc_t size
	);


extern void*
alloc_realloc_h(
	_opaque_ alloc_handle_t* old_handle,
	_opaque_ void* ptr,
	alloc_t old_size,
	_opaque_ alloc_handle_t* new_handle,
	alloc_t new_size,
	int zero
	);


extern void*
alloc_realloc_uh(
	_opaque_ alloc_handle_t* old_handle,
	_opaque_ void* ptr,
	alloc_t old_size,
	_opaque_ alloc_handle_t* new_handle,
	alloc_t new_size,
	int zero
	);


#ifdef __cplusplus
}
#endif

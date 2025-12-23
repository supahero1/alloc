/*
 *   Copyright 2024 Franciszek Balcerak
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

#ifndef ALLOC_DEFAULT_BLOCK_SIZE
	#define ALLOC_DEFAULT_BLOCK_SIZE MACRO_POWER_OF_2(20)
#endif

#ifndef ALLOC_THREAD_LOCAL
	#define ALLOC_THREAD_LOCAL 0
#endif

#include "../include/sync.h"
#include "../include/debug.h"
#include "../include/alloc.h"

#if !defined(NDEBUG) && (defined(VALGRIND) || __has_include(<valgrind/valgrind.h>))
	#define ALLOC_VALGRIND

	#include <valgrind/valgrind.h>
#endif

#ifdef ALLOC_DEBUG
	#include <stdlib.h>
#else
	#include <stdio.h>
#endif

#include <assert.h>
#include <string.h>

#ifndef _packed_
	#define _packed_ __attribute__((packed))
#endif


#ifdef _WIN32
	#include <windows.h>


	private alloc_t
	alloc_read_page_size(
		void
		)
	{
		SYSTEM_INFO info;
		GetSystemInfo(&info);
		return info.dwPageSize;
	}


	#if ALLOC_THREAD_LOCAL == 1
		private DWORD alloc_fls_index;


		private void NTAPI
		alloc_destructor_wrapper(
			void* arg
			)
		{
			void (*cleanup)(void) = arg;
			cleanup();
		}


		private void
		alloc_register_thread_local_cleanup(
			void (*cleanup)(void)
			)
		{
			alloc_fls_index = FlsAlloc(alloc_destructor_wrapper);
			FlsSetValue(alloc_fls_index, cleanup);
		}
	#endif


	_alloc_func_ void*
	alloc_alloc_virtual(
		alloc_t size
		)
	{
		if(!size)
		{
			return NULL;
		}

		return VirtualAlloc(NULL, size,
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	}


	void
	alloc_free_virtual(
		_opaque_ void* ptr,
		alloc_t size
		)
	{
		(void) size;

		if(!ptr)
		{
			return;
		}

		BOOL status = VirtualFree((void*) ptr, 0, MEM_RELEASE);
		assert_neq(status, 0);
	}


	_alloc_func_ void*
	alloc_alloc_virtual_aligned(
		alloc_t size,
		alloc_t alignment,
		_out_ void** ptr
		)
	{
		assert_not_null(ptr);
		assert_ge(alignment, 1);
		assert_eq(MACRO_IS_POWER_OF_2(alignment), 1);

		if(!size)
		{
			*ptr = NULL;
			return NULL;
		}

		alloc_t mask = alignment - 1;
		alloc_t actual_size = size + mask;

		void* real_ptr = VirtualAlloc(NULL,
			actual_size, MEM_RESERVE, PAGE_NOACCESS);
		if(!real_ptr)
		{
			return NULL;
		}

		void* aligned_ptr = MACRO_ALIGN_UP(real_ptr, mask);

		void* committed_ptr = VirtualAlloc(
			aligned_ptr, size, MEM_COMMIT, PAGE_READWRITE);
		if(!committed_ptr)
		{
			alloc_free_virtual(real_ptr, actual_size);
			return NULL;
		}

		*ptr = committed_ptr;
		return real_ptr;
	}


	void
	alloc_free_virtual_aligned(
		_opaque_ void* ptr,
		alloc_t size,
		alloc_t alignment
		)
	{
		alloc_free_virtual(ptr, size + alignment - 1);
	}


	_alloc_func_ void*
	alloc_realloc_virtual(
		_opaque_ void* ptr,
		alloc_t old_size,
		alloc_t new_size
		)
	{
		if(!new_size)
		{
			alloc_free_virtual(ptr, old_size);
			return NULL;
		}

		if(!ptr)
		{
			return alloc_alloc_virtual(new_size);
		}

		if(new_size < old_size)
		{
			alloc_t page_size = alloc_get_page_size();
			void* decommit_start = ptr + MACRO_ALIGN_UP(new_size, page_size - 1);
			alloc_t decommit_size = old_size - (decommit_start - ptr);

			if(decommit_size > 0)
			{
				VirtualFree(decommit_start, decommit_size, MEM_DECOMMIT);
			}

			return ptr;
		}

		void* new_ptr = alloc_alloc_virtual(new_size);
		if(!new_ptr)
		{
			return NULL;
		}

		(void) memcpy(new_ptr, ptr, old_size);
		alloc_free_virtual(ptr, old_size);

		return new_ptr;
	}


#else
	#include <unistd.h>
	#include <sys/mman.h>


	private alloc_t
	alloc_read_page_size(
		void
		)
	{
		return getpagesize();
	}


	#if ALLOC_THREAD_LOCAL == 1
		private pthread_key_t alloc_thread_key;


		private void
		alloc_destructor_wrapper(
			void* arg
			)
		{
			void (*cleanup)(void) = arg;
			cleanup();
		}


		private void
		alloc_register_thread_local_cleanup(
			void (*cleanup)(void)
			)
		{
			pthread_key_create(&alloc_thread_key, alloc_destructor_wrapper);
			pthread_setspecific(alloc_thread_key, cleanup);
		}
	#endif


	_alloc_func_ void*
	alloc_alloc_virtual(
		alloc_t size
		)
	{
		if(!size)
		{
			return NULL;
		}

		void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if(ptr == MAP_FAILED)
		{
			return NULL;
		}

		return ptr;
	}


	void
	alloc_free_virtual(
		_opaque_ void* ptr,
		alloc_t size
		)
	{
		if(!ptr)
		{
			return;
		}

		int status = munmap((void*) ptr, size);
		assert_eq(status, 0);
	}


	_alloc_func_ void*
	alloc_alloc_virtual_aligned(
		alloc_t size,
		alloc_t alignment,
		_out_ void** ptr
		)
	{
		assert_not_null(ptr);
		assert_ge(alignment, 1);
		assert_eq(MACRO_IS_POWER_OF_2(alignment), 1);

		if(!size)
		{
			*ptr = NULL;
			return NULL;
		}

		alloc_t mask = alignment - 1;
		alloc_t actual_size = size + mask;

		void* real_ptr = mmap(NULL, actual_size, PROT_NONE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if(real_ptr == MAP_FAILED)
		{
			return NULL;
		}

		void* aligned_ptr = MACRO_ALIGN_UP(real_ptr, mask);

		if(mprotect(aligned_ptr, size, PROT_READ | PROT_WRITE))
		{
			alloc_free_virtual(real_ptr, actual_size);
			return NULL;
		}

		*ptr = aligned_ptr;
		return real_ptr;
	}


	void
	alloc_free_virtual_aligned(
		_opaque_ void* real_ptr,
		alloc_t size,
		alloc_t alignment
		)
	{
		alloc_free_virtual(real_ptr, size + alignment - 1);
	}


	_alloc_func_ void*
	alloc_realloc_virtual(
		_opaque_ void* ptr,
		alloc_t old_size,
		alloc_t new_size
		)
	{
		if(!new_size)
		{
			alloc_free_virtual(ptr, old_size);
			return NULL;
		}

		if(!ptr)
		{
			return alloc_alloc_virtual(new_size);
		}

		void* new_ptr = mremap((void*) ptr, old_size, new_size, MREMAP_MAYMOVE);
		if(new_ptr == MAP_FAILED)
		{
			return NULL;
		}

		return new_ptr;
	}
#endif


_alloc_func_ void*
alloc_realloc_virtual_aligned(
	_opaque_ void* real_ptr,
	alloc_t old_size,
	alloc_t new_size,
	alloc_t alignment,
	_out_ void** new_ptr
	)
{
	assert_not_null(new_ptr);
	assert_ge(alignment, 1);
	assert_eq(MACRO_IS_POWER_OF_2(alignment), 1);

	if(!new_size)
	{
		alloc_free_virtual_aligned(real_ptr, old_size, alignment);
		*new_ptr = NULL;
		return NULL;
	}

	if(!real_ptr)
	{
		return alloc_alloc_virtual_aligned(new_size, alignment, new_ptr);
	}

	void* new_real_ptr = alloc_alloc_virtual_aligned(new_size, alignment, new_ptr);
	if(!new_real_ptr)
	{
		return NULL;
	}

	void* aligned_old_ptr = MACRO_ALIGN_UP((void*) real_ptr, alignment);
	void* aligned_new_ptr = *new_ptr;

	alloc_t copy_size = MACRO_MIN(old_size, new_size);
	(void) memcpy(aligned_new_ptr, aligned_old_ptr, copy_size);

	alloc_free_virtual_aligned(real_ptr, old_size, alignment);

	return new_real_ptr;
}





typedef struct _packed_ alloc_header
{
	void* prev;
	void* next;
#if ALLOC_THREAD_LOCAL == 1
	void* handle;
#endif
	uint32_t real_ptr_off;
	uint32_t alloc_size;
}
alloc_header_t;


#define ALLOC_1_MAX UINT8_MAX

typedef struct alloc_1 alloc_1_t;

struct _packed_ alloc_1
{
	uint8_t next;
	uint8_t used;
	uint8_t count;
	uint8_t free;
	uint8_t data[];
};


typedef struct alloc_1_block alloc_1_block_t;

struct _packed_ alloc_1_block
{
	alloc_1_block_t* prev;
	alloc_1_block_t* next;
#if ALLOC_THREAD_LOCAL == 1
	void* handle;
#endif
	uint32_t real_ptr_off;
	uint32_t alloc_size;
	uint8_t count;
	uint8_t free;
};


#define ALLOC_2_MAX UINT16_MAX

typedef struct alloc_2 alloc_2_t;

struct _packed_ alloc_2
{
	alloc_2_t* prev;
	alloc_2_t* next;
#if ALLOC_THREAD_LOCAL == 1
	void* handle;
#endif
	uint32_t real_ptr_off;
	uint32_t alloc_size;
	uint16_t used;
	uint16_t count;
	uint16_t free;
};


#define ALLOC_4_MAX UINT32_MAX

typedef struct alloc_4 alloc_4_t;

struct _packed_ alloc_4
{
	alloc_4_t* prev;
	alloc_4_t* next;
#if ALLOC_THREAD_LOCAL == 1
	void* handle;
#endif
	uint32_t real_ptr_off;
	uint32_t alloc_size;
	uint32_t used;
	uint32_t count;
	uint32_t free;
};


typedef struct alloc_handle_impl alloc_handle_impl_t;


typedef void*
(*alloc_alloc_fn_t)(
	alloc_handle_impl_t* handle,
	alloc_t size,
	int zero
	);


typedef void
(*alloc_free_fn_t)(
	alloc_handle_impl_t* handle,
	void* block_ptr,
	void* ptr,
	alloc_t size
	);


struct alloc_handle_impl
{
	sync_mtx_t mtx;

	union
	{
		alloc_t padding;
		alloc_t stride;
	};
	alloc_t allocators;
	alloc_t allocations;
	alloc_t alloc_limit;
	alloc_t alloc_size;
	alloc_t block_size;

	alloc_handle_flag_t flags;

	alloc_header_t* head;

	alloc_alloc_fn_t alloc_fn;
	alloc_free_fn_t free_fn;
};

static_assert(sizeof(alloc_handle_t) >= sizeof(alloc_handle_impl_t),
	"alloc_handle_t size mismatch");


private alloc_handle_info_t alloc_default_handle_info[] =
(alloc_handle_info_t[])
{
/*   0*/{ .alloc_size = 1, .block_size =
			sizeof(alloc_1_block_t) + sizeof(alloc_1_t), .alignment = 1 },
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(2)
/*   1*/{ .alloc_size = MACRO_POWER_OF_2(1), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(1) },
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(3)
/*   2*/{ .alloc_size = MACRO_POWER_OF_2(2), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(2)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(4)
/*   3*/{ .alloc_size = MACRO_POWER_OF_2(3), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(3)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(5)
/*   4*/{ .alloc_size = MACRO_POWER_OF_2(4), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(4)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(6)
/*   5*/{ .alloc_size = MACRO_POWER_OF_2(5), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(5)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(7)
/*   6*/{ .alloc_size = MACRO_POWER_OF_2(6), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(6)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(8)
/*   7*/{ .alloc_size = MACRO_POWER_OF_2(7), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(7)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(9)
/*   8*/{ .alloc_size = MACRO_POWER_OF_2(8), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(8)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(10)
/*   9*/{ .alloc_size = MACRO_POWER_OF_2(9), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(9)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(11)
/*  10*/{ .alloc_size = MACRO_POWER_OF_2(10), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(10)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(12)
/*  11*/{ .alloc_size = MACRO_POWER_OF_2(11), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(11)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(13)
/*  12*/{ .alloc_size = MACRO_POWER_OF_2(12), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(12)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(14)
/*  13*/{ .alloc_size = MACRO_POWER_OF_2(13), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(13)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(15)
/*  14*/{ .alloc_size = MACRO_POWER_OF_2(14), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(14)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(16)
/*  15*/{ .alloc_size = MACRO_POWER_OF_2(15), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(15)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(17)
/*  16*/{ .alloc_size = MACRO_POWER_OF_2(16), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(16)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(18)
/*  17*/{ .alloc_size = MACRO_POWER_OF_2(17), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(17)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(19)
/*  18*/{ .alloc_size = MACRO_POWER_OF_2(18), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(18)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(20)
/*  19*/{ .alloc_size = MACRO_POWER_OF_2(19), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(19)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(21)
/*  20*/{ .alloc_size = MACRO_POWER_OF_2(20), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(20)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(22)
/*  21*/{ .alloc_size = MACRO_POWER_OF_2(21), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(21)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(23)
/*  22*/{ .alloc_size = MACRO_POWER_OF_2(22), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(22)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(24)
/*  23*/{ .alloc_size = MACRO_POWER_OF_2(23), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(23)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(25)
/*  24*/{ .alloc_size = MACRO_POWER_OF_2(24), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(24)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(26)
/*  25*/{ .alloc_size = MACRO_POWER_OF_2(25), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(25)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(27)
/*  26*/{ .alloc_size = MACRO_POWER_OF_2(26), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(26)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(28)
/*  27*/{ .alloc_size = MACRO_POWER_OF_2(27), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(27)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= MACRO_POWER_OF_2(29)
/*  28*/{ .alloc_size = MACRO_POWER_OF_2(28), .block_size =
			ALLOC_DEFAULT_BLOCK_SIZE, .alignment = MACRO_POWER_OF_2(28)},
#endif
};

private alloc_state_info_t alloc_default_state_info =
(alloc_state_info_t)
{
	.handles = alloc_default_handle_info,
	.handle_count = MACRO_ARRAY_LEN(alloc_default_handle_info),
	.idx_fn = NULL
};


private alloc_t alloc_page_size;
private alloc_t alloc_page_size_mask;
private uint32_t alloc_page_size_shift;
private
#if ALLOC_THREAD_LOCAL == 1
	thread_local
#endif
	const alloc_state_t* alloc_global_state;





private void
alloc_library_cleanup(
	void
	)
{
	alloc_free_state(alloc_global_state);
	alloc_global_state = NULL;
}


private assert_ctor void
alloc_library_init(
	void
	)
{
	alloc_page_size = alloc_read_page_size();

	assert_neq(alloc_page_size, 0);
	assert_true(MACRO_IS_POWER_OF_2(alloc_page_size));

	alloc_page_size_mask = alloc_page_size - 1;
	alloc_page_size_shift = MACRO_LOG2(alloc_page_size);

#if ALLOC_THREAD_LOCAL == 1
	alloc_register_thread_local_cleanup(alloc_library_cleanup);
#endif
}


#if ALLOC_THREAD_LOCAL != 1


	private assert_dtor void
	alloc_library_free(
		void
		)
	{
		alloc_library_cleanup();
	}


#endif


_const_func_ const alloc_state_t*
alloc_get_global_state(
	void
	)
{
	if(!alloc_global_state)
	{
		alloc_global_state = alloc_alloc_state(&alloc_default_state_info);
		assert_not_null(alloc_global_state);
	}

	return alloc_global_state;
}


_const_func_ alloc_t
alloc_get_page_size(
	void
	)
{
	return alloc_page_size;
}


_const_func_ alloc_t
alloc_get_default_block_size(
	void
	)
{
	return ALLOC_DEFAULT_BLOCK_SIZE;
}


private void*
alloc_alloc_1_fn(
	alloc_handle_impl_t* handle,
	alloc_t size,
	int zero
	)
{
	(void) size;

	alloc_t stride = handle->stride;
	alloc_t capacity = stride - sizeof(alloc_1_t);

	alloc_1_block_t* block = (void*) handle->head;
	if(!block)
	{
		void* real_ptr = alloc_alloc_virtual_aligned(
			handle->block_size, handle->block_size, (void**) &block);
		if(!real_ptr)
		{
			return NULL;
		}

		assert_lt((void*) block - real_ptr, UINT32_MAX);

#if ALLOC_THREAD_LOCAL == 1
		block->handle = handle;
#endif

		block->real_ptr_off = (void*) block - real_ptr;
		block->alloc_size = 1;

		void* base = (void*) block + sizeof(alloc_1_block_t);
		alloc_1_t* alloc = base;

		for(alloc_t i = 0; i < handle->alloc_limit - 1; ++i)
		{
			alloc->next = i + 1;
			alloc->free = ALLOC_1_MAX;

			alloc = (void*) alloc + stride;
		}

		alloc->next = ALLOC_1_MAX;
		alloc->free = ALLOC_1_MAX;

		++handle->allocators;
		handle->head = (void*) block;
	}

	void* base = (void*) block + sizeof(alloc_1_block_t);
	alloc_1_t* alloc = base + block->free * stride;

	++handle->allocations;
	++block->count;
	++alloc->count;

	if(alloc->count == capacity)
	{
		if(block->count == capacity * handle->alloc_limit)
		{
			handle->head = (void*) block->next;

			if(block->next)
			{
				block->next->prev = NULL;
			}

			block->prev = NULL;
			block->next = NULL;
		}
		else
		{
			block->free = alloc->next;
		}
	}

	if(alloc->free != UINT8_MAX)
	{
		uint8_t* ptr = alloc->data + alloc->free;

#ifdef ALLOC_VALGRIND
		VALGRIND_DISABLE_ERROR_REPORTING;
#endif

		alloc->free = *ptr;

		if(zero)
		{
			*ptr = 0;
		}

#ifdef ALLOC_VALGRIND
		VALGRIND_ENABLE_ERROR_REPORTING;
		VALGRIND_MALLOCLIKE_BLOCK(ptr, 1, 0, zero);
#endif

		return ptr;
	}

	uint8_t* ptr = alloc->data + alloc->used++;

#ifdef ALLOC_VALGRIND
	VALGRIND_MALLOCLIKE_BLOCK(ptr, 1, 0, 1);
#endif

	return ptr;
}


private void
alloc_free_1_fn(
	alloc_handle_impl_t* handle,
	void* block_ptr,
	void* ptr,
	alloc_t size
	)
{
	(void) size;

	alloc_t stride = handle->stride;
	alloc_t capacity = stride - sizeof(alloc_1_t);

	alloc_1_block_t* block = block_ptr;
	void* base = (void*) block + sizeof(alloc_1_block_t);

	alloc_t offset = ptr - base;
	alloc_t sub_block_idx = offset / stride;
	alloc_1_t* alloc = (void*)(base + sub_block_idx * stride);

	--handle->allocations;
	--block->count;
	--alloc->count;

	if(
		block->count == 0 &&
		(
			(handle->flags & ALLOC_HANDLE_FLAG_IMMEDIATE_FREE) ||
			(
				handle->allocators >= 2 &&
				!(handle->flags & ALLOC_HANDLE_FLAG_DO_NOT_FREE) &&
				handle->allocations <= capacity *
					handle->alloc_limit * (handle->allocators - 2)
			)
		)
		)
	{
		if(block->prev)
		{
			block->prev->next = block->next;
		}
		else
		{
			handle->head = (void*) block->next;
		}

		if(block->next)
		{
			block->next->prev = block->prev;
		}

		alloc_free_virtual_aligned((void*) block - block->real_ptr_off,
			handle->block_size, handle->block_size);

		--handle->allocators;
	}
	else
	{
		if(alloc->count == capacity - 1)
		{
			alloc->next = block->free;
			block->free = sub_block_idx;

			if(block->count == capacity * handle->alloc_limit - 1)
			{
				if(handle->head)
				{
					handle->head->prev = block;
				}

				assert_null(block->prev);
				block->next = (void*) handle->head;
				handle->head = (void*) block;
			}
		}


		*((uint8_t*) ptr) = alloc->free;
		alloc->free = (uint8_t*) ptr - alloc->data;
	}
}


private void*
alloc_alloc_2_fn(
	alloc_handle_impl_t* handle,
	alloc_t size,
	int zero
	)
{
	(void) size;

	alloc_2_t* alloc = (void*) handle->head;
	if(!alloc)
	{
		void* real_ptr = alloc_alloc_virtual_aligned(
			handle->block_size, handle->block_size, (void**) &alloc);
		if(!real_ptr)
		{
			return NULL;
		}

		assert_lt((void*) alloc - real_ptr, UINT32_MAX);

#if ALLOC_THREAD_LOCAL == 1
		alloc->handle = handle;
#endif

		alloc->real_ptr_off = (void*) alloc - real_ptr;
		alloc->alloc_size = handle->alloc_size;

		alloc->free = ALLOC_2_MAX;

		++handle->allocators;
		handle->head = (void*) alloc;
	}

	++handle->allocations;
	++alloc->count;

	void* data = (void*) alloc + handle->padding;

	if(alloc->count == handle->alloc_limit)
	{
		handle->head = (void*) alloc->next;

		if(alloc->next)
		{
			alloc->next->prev = NULL;
		}

		alloc->next = NULL;
	}

	if(alloc->free != ALLOC_2_MAX)
	{
		void* ptr = data + alloc->free * handle->alloc_size;

#ifdef ALLOC_VALGRIND
		VALGRIND_DISABLE_ERROR_REPORTING;
#endif

		(void) memcpy(&alloc->free, ptr, 2);

		if(zero)
		{
			(void) memset(ptr, 0, handle->alloc_size);
		}

#ifdef ALLOC_VALGRIND
		VALGRIND_ENABLE_ERROR_REPORTING;
		VALGRIND_MALLOCLIKE_BLOCK(ptr, handle->alloc_size, 0, zero);
#endif

		return ptr;
	}

	void* ptr = data + alloc->used++ * handle->alloc_size;

#ifdef ALLOC_VALGRIND
	VALGRIND_MALLOCLIKE_BLOCK(ptr, handle->alloc_size, 0, 1);
#endif

	return ptr;
}


private void
alloc_free_2_fn(
	alloc_handle_impl_t* handle,
	void* block_ptr,
	void* ptr,
	alloc_t size
	)
{
	(void) size;

	alloc_2_t* alloc = block_ptr;

	--handle->allocations;
	--alloc->count;

	if(
		alloc->count == 0 &&
		(
			(handle->flags & ALLOC_HANDLE_FLAG_IMMEDIATE_FREE) ||
			(
				handle->allocators >= 2 &&
				!(handle->flags & ALLOC_HANDLE_FLAG_DO_NOT_FREE) &&
				handle->allocations <=
					handle->alloc_limit * (handle->allocators - 2)
			)
		)
		)
	{
		if(alloc->prev)
		{
			alloc->prev->next = alloc->next;
		}
		else
		{
			handle->head = (void*) alloc->next;
		}

		if(alloc->next)
		{
			alloc->next->prev = alloc->prev;
		}

		alloc_free_virtual_aligned((void*) alloc - alloc->real_ptr_off,
			handle->block_size, handle->block_size);

		--handle->allocators;
	}
	else
	{
		if(alloc->count == handle->alloc_limit - 1)
		{
			if(handle->head)
			{
				handle->head->prev = alloc;
			}

			assert_null(alloc->prev);
			alloc->next = (void*) handle->head;
			handle->head = (void*) alloc;
		}


		(void) memcpy(ptr, &alloc->free, 2);

		void* data = (void*) alloc + handle->padding;
		alloc->free = (ptr - data) / handle->alloc_size;
	}
}


private void*
alloc_alloc_4_fn(
	alloc_handle_impl_t* handle,
	alloc_t size,
	int zero
	)
{
	(void) size;

	alloc_4_t* alloc = (void*) handle->head;
	if(!alloc)
	{
		void* real_ptr = alloc_alloc_virtual_aligned(
			handle->block_size, handle->block_size, (void**) &alloc);
		if(!real_ptr)
		{
			return NULL;
		}

		assert_lt((void*) alloc - real_ptr, UINT32_MAX);

#if ALLOC_THREAD_LOCAL == 1
		alloc->handle = handle;
#endif

		alloc->real_ptr_off = (void*) alloc - real_ptr;
		alloc->alloc_size = handle->alloc_size;

		alloc->free = ALLOC_4_MAX;

		++handle->allocators;
		handle->head = (void*) alloc;
	}

	++handle->allocations;
	++alloc->count;

	void* data = (void*) alloc + handle->padding;

	if(alloc->count == handle->alloc_limit)
	{
		handle->head = (void*) alloc->next;

		if(alloc->next)
		{
			alloc->next->prev = NULL;
		}

		alloc->next = NULL;
	}

	if(alloc->free != ALLOC_4_MAX)
	{
		assert_lt(alloc->free, handle->alloc_limit);

		void* ptr = data + alloc->free * handle->alloc_size;

#ifdef ALLOC_VALGRIND
		VALGRIND_DISABLE_ERROR_REPORTING;
#endif

		(void) memcpy(&alloc->free, ptr, 4);

		if(zero)
		{
			(void) memset(ptr, 0, handle->alloc_size);
		}

#ifdef ALLOC_VALGRIND
		VALGRIND_ENABLE_ERROR_REPORTING;
		VALGRIND_MALLOCLIKE_BLOCK(ptr, handle->alloc_size, 0, zero);
#endif

		return ptr;
	}

	void* ptr = data + alloc->used++ * handle->alloc_size;

#ifdef ALLOC_VALGRIND
	VALGRIND_MALLOCLIKE_BLOCK(ptr, handle->alloc_size, 0, 1);
#endif

	return ptr;
}


private void
alloc_free_4_fn(
	alloc_handle_impl_t* handle,
	void* block_ptr,
	void* ptr,
	alloc_t size
	)
{
	(void) size;

	alloc_4_t* alloc = block_ptr;

	--handle->allocations;
	--alloc->count;

	if(
		alloc->count == 0 &&
		(
			(handle->flags & ALLOC_HANDLE_FLAG_IMMEDIATE_FREE) ||
			(
				handle->allocators >= 2 &&
				!(handle->flags & ALLOC_HANDLE_FLAG_DO_NOT_FREE) &&
				handle->allocations <=
					handle->alloc_limit * (handle->allocators - 2)
			)
		)
		)
	{
		if(alloc->prev)
		{
			alloc->prev->next = alloc->next;
		}
		else
		{
			handle->head = (void*) alloc->next;
		}

		if(alloc->next)
		{
			alloc->next->prev = alloc->prev;
		}

		alloc_free_virtual_aligned((void*) alloc - alloc->real_ptr_off,
			handle->block_size, handle->block_size);

		--handle->allocators;
	}
	else
	{
		if(alloc->count == handle->alloc_limit - 1)
		{
			if(handle->head)
			{
				handle->head->prev = alloc;
			}

			assert_null(alloc->prev);
			alloc->next = (void*) handle->head;
			handle->head = (void*) alloc;
		}


		(void) memcpy(ptr, &alloc->free, 4);

		void* data = (void*) alloc + handle->padding;
		alloc->free = (ptr - data) / handle->alloc_size;
	}
}


private void*
alloc_alloc_virtual_fn(
	alloc_handle_impl_t* handle,
	alloc_t size,
	int zero
	)
{
	(void) handle;
	(void) zero;

	void* ptr = alloc_alloc_virtual(size);

#ifdef ALLOC_VALGRIND
	VALGRIND_MALLOCLIKE_BLOCK(ptr, size, 0, 1);
#endif

	return ptr;
}


private void
alloc_free_virtual_fn(
	alloc_handle_impl_t* handle,
	void* block_ptr,
	void* ptr,
	alloc_t size
	)
{
	(void) handle;

	assert_eq(block_ptr, ptr);

	alloc_free_virtual(ptr, size);
}


private int
alloc_handle_is_virtual(
	_in_ alloc_handle_impl_t* handle
	)
{
	return !handle->block_size;
}


void
alloc_create_handle(
	_in_ alloc_handle_info_t* info,
	_opaque_ alloc_handle_t* handle
	)
{
	assert_not_null(handle);

	alloc_handle_impl_t* handle_impl = (void*) handle;

	sync_mtx_init(&handle_impl->mtx);

	handle_impl->allocators = 0;
	handle_impl->allocations = 0;

	handle_impl->head = NULL;

	handle_impl->flags = ALLOC_HANDLE_FLAG_NONE;


	if(!info)
	{
		handle_impl->padding = 0;
		handle_impl->alloc_limit = 0;
		handle_impl->alloc_size = 0;
		handle_impl->block_size = 0;

		handle_impl->alloc_fn = alloc_alloc_virtual_fn;
		handle_impl->free_fn = alloc_free_virtual_fn;

		return;
	}


	assert_neq(info->alignment, 0);
	assert_eq(MACRO_IS_POWER_OF_2(info->alignment), 1);


	static const alloc_t block_size_max[] =
	(const alloc_t[])
	{
		0,
		65536,
		131072,
		131072,
		1073741824
	};

	static const alloc_t alloc_limit_max[] =
	(const alloc_t[])
	{
		0,
		UINT8_MAX - 2,
		UINT16_MAX - 2,
		UINT16_MAX - 2,
		UINT32_MAX - 2
	};

	static const alloc_alloc_fn_t alloc_fns[] =
	(const alloc_alloc_fn_t[])
	{
		NULL,
		alloc_alloc_1_fn,
		alloc_alloc_2_fn,
		alloc_alloc_2_fn,
		alloc_alloc_4_fn
	};

	static const alloc_free_fn_t free_fns[] =
	(const alloc_free_fn_t[])
	{
		NULL,
		alloc_free_1_fn,
		alloc_free_2_fn,
		alloc_free_2_fn,
		alloc_free_4_fn
	};

	static const uint32_t header_sizes[] =
	(const uint32_t[])
	{
		0,
		sizeof(alloc_1_block_t),
		sizeof(alloc_2_t),
		sizeof(alloc_2_t),
		sizeof(alloc_4_t)
	};

	alloc_t table_idx = MACRO_MIN(info->alloc_size, 4U);


	if(info->alloc_size == 1)
	{
		alloc_t block_size = info->block_size;
		block_size = MACRO_MIN(block_size, block_size_max[table_idx]);
		block_size = MACRO_MAX(block_size, alloc_page_size);
		block_size = MACRO_NEXT_OR_EQUAL_POWER_OF_2(block_size);

		alloc_t avail_bytes = block_size - sizeof(alloc_1_block_t);
		alloc_t limit_max = alloc_limit_max[table_idx];

		alloc_t k_a = ALLOC_1_MAX - 1;
		alloc_t stride_a = sizeof(alloc_1_t) + k_a;
		alloc_t n_a = avail_bytes / stride_a;

		n_a = MACRO_MIN(n_a, limit_max);

		alloc_t total_a = n_a * k_a;

		alloc_t n_b = n_a + 1;
		alloc_t total_b = 0;
		alloc_t k_b = 0;

		if(n_b <= limit_max)
		{
			alloc_t stride_b = avail_bytes / n_b;

			if(stride_b > sizeof(alloc_1_t))
			{
				k_b = stride_b - sizeof(alloc_1_t);
				total_b = n_b * k_b;
			}
		}

		alloc_t best_n;
		alloc_t best_k;

		if(total_b > total_a)
		{
			best_n = n_b;
			best_k = k_b;
		}
		else
		{
			best_n = n_a;
			best_k = k_a;
		}

		handle_impl->stride = best_k + sizeof(alloc_1_t);
		handle_impl->alloc_limit = best_n;
		handle_impl->alloc_size = 1;
		handle_impl->block_size = block_size;

		handle_impl->alloc_fn = alloc_fns[table_idx];
		handle_impl->free_fn = free_fns[table_idx];

		return;
	}


	alloc_t header_size = header_sizes[table_idx];
	alloc_t mask = info->alignment - 1;
	alloc_t padding = (header_size + mask) & ~mask;

	alloc_t block_size = info->block_size;
	block_size = MACRO_MIN(block_size, block_size_max[table_idx]);
	block_size = MACRO_MAX(block_size, alloc_page_size);
	block_size = MACRO_NEXT_OR_EQUAL_POWER_OF_2(block_size);

	alloc_t alloc_limit = (block_size - padding) / info->alloc_size;
	alloc_limit = MACRO_MIN(alloc_limit, alloc_limit_max[table_idx]);
	alloc_limit = MACRO_MAX(alloc_limit, 1U);

	block_size = padding + alloc_limit * info->alloc_size;
	block_size = MACRO_NEXT_OR_EQUAL_POWER_OF_2(block_size);

	handle_impl->padding = padding;
	handle_impl->alloc_limit = alloc_limit;
	handle_impl->alloc_size = info->alloc_size;
	handle_impl->block_size = block_size;

	handle_impl->alloc_fn = alloc_fns[table_idx];
	handle_impl->free_fn = free_fns[table_idx];
}


void
alloc_clone_handle(
	_opaque_ alloc_handle_t* source,
	_opaque_ alloc_handle_t* handle
	)
{
	assert_not_null(source);
	assert_not_null(handle);

	alloc_handle_impl_t* source_impl = (void*) source;

	alloc_handle_info_t info =
	{
		.alloc_size = source_impl->alloc_size,
		.block_size = source_impl->block_size,
		.alignment = source_impl->padding
	};

	alloc_create_handle(&info, handle);
}


void
alloc_free_handle(
	_opaque_ alloc_handle_t* handle
	)
{
	assert_not_null(handle);

	alloc_handle_impl_t* handle_impl = (void*) handle;

	if(handle_impl->head)
	{
		alloc_free_virtual_aligned(
			(void*) handle_impl->head - handle_impl->head->real_ptr_off,
			handle_impl->block_size, handle_impl->block_size
			);
	}

	sync_mtx_free(&handle_impl->mtx);
}


private uint32_t
alloc_default_idx_fn(
	alloc_t size
	)
{
	return MACRO_LOG2(MACRO_NEXT_OR_EQUAL_POWER_OF_2(size));
}


_alloc_func_ const alloc_state_t*
alloc_alloc_state(
	_in_ alloc_state_info_t* info
	)
{
	if(!info)
	{
		info = &alloc_default_state_info;
	}


	alloc_t handle_count = info->handle_count + 1;
	alloc_state_t* state = alloc_alloc_virtual(
		sizeof(alloc_state_t) + sizeof(alloc_handle_t) * handle_count);
	if(!state)
	{
		return NULL;
	}

	if(!info->idx_fn)
	{
		state->idx_fn = alloc_default_idx_fn;
	}
	else
	{
		state->idx_fn = info->idx_fn;
	}

	state->handle_count = handle_count;


	alloc_handle_info_t* handle_info = info->handles;
	alloc_handle_info_t* handle_info_end = handle_info + info->handle_count;

	alloc_handle_t* handle = state->handles;

	for(; handle_info < handle_info_end; ++handle_info, ++handle)
	{
		alloc_create_handle(handle_info, handle);

#if ALLOC_THREAD_LOCAL == 1
		alloc_handle_impl_t* handle_impl = (void*) handle;
		handle_impl->flags = ALLOC_HANDLE_FLAG_THREAD_LOCAL;
#endif
	}

	alloc_create_handle(NULL, handle);

#if ALLOC_THREAD_LOCAL == 1
	alloc_handle_impl_t* handle_impl = (void*) handle;
	handle_impl->flags = ALLOC_HANDLE_FLAG_THREAD_LOCAL;
#endif


	return state;
}


_alloc_func_ const alloc_state_t*
alloc_clone_state(
	_in_ alloc_state_t* source
	)
{
	assert_not_null(source);

	alloc_t handle_count = source->handle_count;
	alloc_t total_size = sizeof(alloc_state_t) + sizeof(alloc_handle_t) * handle_count;

	alloc_state_t* state = alloc_alloc_virtual(total_size);
	if(!state)
	{
		return NULL;
	}

	(void) memcpy(state, source, total_size);

	alloc_handle_impl_t* handle = (void*) state->handles;
	alloc_handle_impl_t* handle_end = handle + handle_count;

	for(; handle < handle_end; ++handle)
	{
		handle->allocators = 0;
		handle->allocations = 0;

		handle->flags = 0;

		handle->head = NULL;
	}

	return state;
}


void
alloc_free_state(
	_opaque_ alloc_state_t* state
	)
{
	if(!state)
	{
		state = alloc_global_state;
		if(!state)
		{
			return;
		}
	}


	alloc_t i = 0;
	alloc_t handle_count = state->handle_count;

	for(; i < handle_count; ++i)
	{
		alloc_free_handle(&state->handles[i]);
	}

	alloc_free_virtual(state, sizeof(alloc_state_t) +
		handle_count * sizeof(alloc_handle_impl_t));
}


_pure_func_ _opaque_ alloc_handle_t*
alloc_get_handle_s(
	_in_ alloc_state_t* state,
	alloc_t size
	)
{
	assert_not_null(state);

	if(size == 0)
	{
		return NULL;
	}

	uint32_t idx = state->idx_fn(size);
	idx = MACRO_MIN(idx, state->handle_count - 1);

	return &state->handles[idx];
}


void
alloc_handle_lock_h(
	_opaque_ alloc_handle_t* handle
	)
{
	assert_not_null(handle);

	alloc_handle_impl_t* handle_impl = (void*) handle;

	sync_mtx_lock(&handle_impl->mtx);
}


void
alloc_handle_unlock_h(
	_opaque_ alloc_handle_t* handle
	)
{
	assert_not_null(handle);

	alloc_handle_impl_t* handle_impl = (void*) handle;

	sync_mtx_unlock(&handle_impl->mtx);
}


void
alloc_handle_set_flags_h(
	_opaque_ alloc_handle_t* handle,
	alloc_handle_flag_t flags
	)
{
	assert_not_null(handle);

	alloc_handle_lock_h(handle);
		alloc_handle_set_flags_uh(handle, flags);
	alloc_handle_unlock_h(handle);
}


void
alloc_handle_set_flags_uh(
	_opaque_ alloc_handle_t* handle,
	alloc_handle_flag_t flags
	)
{
	assert_not_null(handle);

	alloc_handle_impl_t* handle_impl = (void*) handle;

	handle_impl->flags = flags;
}


void
alloc_handle_add_flags_h(
	_opaque_ alloc_handle_t* handle,
	alloc_handle_flag_t flags
	)
{
	assert_not_null(handle);

	alloc_handle_lock_h(handle);
		alloc_handle_add_flags_uh(handle, flags);
	alloc_handle_unlock_h(handle);
}


void
alloc_handle_add_flags_uh(
	_opaque_ alloc_handle_t* handle,
	alloc_handle_flag_t flags
	)
{
	assert_not_null(handle);

	alloc_handle_impl_t* handle_impl = (void*) handle;

	handle_impl->flags |= flags;
}


void
alloc_handle_del_flags_h(
	_opaque_ alloc_handle_t* handle,
	alloc_handle_flag_t flags
	)
{
	assert_not_null(handle);

	alloc_handle_lock_h(handle);
		alloc_handle_del_flags_uh(handle, flags);
	alloc_handle_unlock_h(handle);
}


void
alloc_handle_del_flags_uh(
	_opaque_ alloc_handle_t* handle,
	alloc_handle_flag_t flags
	)
{
	assert_not_null(handle);

	alloc_handle_impl_t* handle_impl = (void*) handle;

	handle_impl->flags &= ~flags;
}


alloc_handle_flag_t
alloc_handle_get_flags_h(
	_opaque_ alloc_handle_t* handle
	)
{
	assert_not_null(handle);

	alloc_handle_flag_t flags;

	alloc_handle_lock_h(handle);
		flags = alloc_handle_get_flags_uh(handle);
	alloc_handle_unlock_h(handle);

	return flags;
}


alloc_handle_flag_t
alloc_handle_get_flags_uh(
	_opaque_ alloc_handle_t* handle
	)
{
	assert_not_null(handle);

	alloc_handle_impl_t* handle_impl = (void*) handle;

	return handle_impl->flags;
}


_alloc_func_ void*
alloc_alloc_l(
	alloc_t size,
	int zero
	)
{
	if(!size)
	{
		return NULL;
	}

#ifndef ALLOC_DEBUG
	const alloc_state_t* state = alloc_get_global_state();
	const alloc_handle_t* handle = alloc_get_handle_s(state, size);

	return alloc_alloc_h(handle, size, zero);
#else
	return zero ? calloc(1, size) : malloc(size);
#endif
}


_alloc_func_ void*
alloc_alloc_ul(
	alloc_t size,
	int zero
	)
{
	if(!size)
	{
		return NULL;
	}

#ifndef ALLOC_DEBUG
	const alloc_state_t* state = alloc_get_global_state();
	const alloc_handle_t* handle = alloc_get_handle_s(state, size);

	return alloc_alloc_uh(handle, size, zero);
#else
	return zero ? calloc(1, size) : malloc(size);
#endif
}


private void*
alloc_get_header(
	const alloc_handle_impl_t* handle,
	_in_ void* ptr
	)
{
	if(alloc_handle_is_virtual(handle))
	{
		return (void*) ptr;
	}

	return MACRO_ALIGN_DOWN((void*) ptr, handle->block_size - 1);
}


_alloc_func_ void*
alloc_alloc_h(
	_opaque_ alloc_handle_t* handle,
	alloc_t size,
	int zero
	)
{
	if(!size)
	{
		return NULL;
	}

	assert_not_null(handle);

	void* ptr;

	alloc_handle_lock_h(handle);
		ptr = alloc_alloc_uh(handle, size, zero);
	alloc_handle_unlock_h(handle);

	return ptr;
}


_alloc_func_ void*
alloc_alloc_uh(
	_opaque_ alloc_handle_t* handle,
	alloc_t size,
	int zero
	)
{
	if(!size)
	{
		return NULL;
	}

	assert_not_null(handle);

#ifndef ALLOC_DEBUG
	alloc_handle_impl_t* handle_impl = (void*) handle;

	return handle_impl->alloc_fn(handle_impl, size, zero);
#else
	return zero ? calloc(1, size) : malloc(size);
#endif
}


private _pure_func_ _opaque_ alloc_handle_t*
alloc_get_handle(
	_opaque_ void* ptr,
	alloc_t size
	)
{
	assert_ptr(ptr, size);

	if(!ptr)
	{
		return NULL;
	}

	const alloc_state_t* state = alloc_get_global_state();
	const alloc_handle_t* handle = alloc_get_handle_s(state, size);

#if ALLOC_THREAD_LOCAL == 1
	if(!alloc_handle_is_virtual((void*) handle))
	{
		const alloc_header_t* header = alloc_get_header((void*) handle, ptr);
		assert_not_null(header->handle);

		assert_eq(((const alloc_handle_impl_t*) header->handle)->flags
			& ALLOC_HANDLE_FLAG_THREAD_LOCAL,
			ALLOC_HANDLE_FLAG_THREAD_LOCAL,
			fprintf(stderr, "alloc_get_handle(): received a pointer "
				"that was not allocated with the default global state\n")
			);

		return header->handle;
	}
#endif

	return handle;
}


void
alloc_free_l(
	_opaque_ void* ptr,
	alloc_t size
	)
{
#ifndef ALLOC_DEBUG
	assert_ptr(ptr, size);

	if(!ptr)
	{
		return;
	}

	const alloc_handle_t* handle = alloc_get_handle(ptr, size);
	alloc_free_h(handle, ptr, size);
#else
	free((void*) ptr);
#endif
}


void
alloc_free_ul(
	_opaque_ void* ptr,
	alloc_t size
	)
{
#ifndef ALLOC_DEBUG
	assert_ptr(ptr, size);

	if(!ptr)
	{
		return;
	}

	const alloc_handle_t* handle = alloc_get_handle(ptr, size);
	alloc_free_uh(handle, ptr, size);
#else
	free((void*) ptr);
#endif
}


#undef ALLOC_FREE


void
alloc_free_h(
	_opaque_ alloc_handle_t* handle,
	_opaque_ void* ptr,
	alloc_t size
	)
{
	assert_ptr(ptr, size);

	if(!ptr)
	{
		return;
	}

	assert_not_null(handle);

	alloc_handle_lock_h(handle);
		alloc_free_uh(handle, ptr, size);
	alloc_handle_unlock_h(handle);
}


void
alloc_free_uh(
	_opaque_ alloc_handle_t* handle,
	_opaque_ void* ptr,
	alloc_t size
	)
{
	assert_ptr(ptr, size);

	if(!ptr)
	{
		return;
	}

	assert_not_null(handle);

#ifndef ALLOC_DEBUG
	alloc_handle_impl_t* handle_impl = (void*) handle;
	alloc_header_t* header = alloc_get_header(handle_impl, ptr);

	assert_eq((uintptr_t) ptr & MACRO_POWER_OF_2_MASK(size), 0,
		{
			if(alloc_handle_is_virtual(handle_impl)) break;
			char format[256];
			snprintf(format, sizeof(format), "alloc_free(): invalid "
				"pointer alignment, got ptr = %s and size = %s\n",
				MACRO_FORMAT_TYPE(ptr), MACRO_FORMAT_TYPE(size));
			fprintf(stderr, format, ptr, size);
		}
		);

	assert_eq(header->alloc_size, handle_impl->alloc_size,
		{
			if(alloc_handle_is_virtual(handle_impl)) break;
			char format[256];
			snprintf(format, sizeof(format), "alloc_free(): mismatch "
				"between passed size %s and (next or equal power of 2)"
				" pointer size %s\n", MACRO_FORMAT_TYPE(size),
				MACRO_FORMAT_TYPE(header->alloc_size));
			fprintf(stderr, format, size, header->alloc_size);
		}
		);

	handle_impl->free_fn(handle_impl, header, (void*) ptr, size);

#ifdef ALLOC_VALGRIND
	VALGRIND_FREELIKE_BLOCK(ptr, 0);
#endif
#else
	free((void*) ptr);
#endif
}


#ifndef ALLOC_DEBUG
	#define ALLOC_REALLOC(alloc_fn, free_fn)							\
	do																	\
	{																	\
		assert_ptr(ptr, old_size);										\
		assert_not_null(old_handle);									\
		assert_not_null(new_handle);									\
																		\
		if(!new_size)													\
		{																\
			free_fn(old_handle, ptr, old_size);							\
			return NULL;												\
		}																\
																		\
		if(!ptr)														\
		{																\
			return alloc_fn(new_handle, new_size, zero);				\
		}																\
																		\
		if(old_handle == new_handle)									\
		{																\
			if(alloc_handle_is_virtual((void*) old_handle))				\
			{															\
				return alloc_realloc_virtual(ptr, old_size, new_size);	\
			}															\
																		\
			if(new_size > old_size && zero)								\
			{															\
				(void) memset((void*) ptr								\
					+ old_size, 0, new_size - old_size);				\
			}															\
																		\
			return (void*) ptr;											\
		}																\
																		\
		void* new_ptr = alloc_fn(new_handle, new_size, zero);			\
		if(!new_ptr)													\
		{																\
			return NULL;												\
		}																\
																		\
		(void) memcpy(new_ptr, ptr, MACRO_MIN(old_size, new_size));		\
																		\
		free_fn(old_handle, ptr, old_size);								\
																		\
		return new_ptr;													\
	}																	\
	while(0)
#else
	#define ALLOC_REALLOC(alloc_fn, free_fn)							\
	do																	\
	{																	\
		(void) old_handle;												\
		(void) new_handle;												\
																		\
		void* new_ptr = realloc((void*) ptr, new_size);					\
																		\
		if(zero && new_size > old_size && new_ptr)						\
		{																\
			(void) memset(new_ptr + old_size, 0, new_size - old_size);	\
		}																\
																		\
		return new_ptr;													\
	}																	\
	while(0)
#endif


void*
alloc_realloc_l(
	_opaque_ void* ptr,
	alloc_t old_size,
	alloc_t new_size,
	int zero
	)
{
	const alloc_state_t* state = alloc_get_global_state();
	const alloc_handle_t* old_handle = alloc_get_handle(ptr, old_size);
	const alloc_handle_t* new_handle = alloc_get_handle_s(state, new_size);

	ALLOC_REALLOC(alloc_alloc_h, alloc_free_h);
}


void*
alloc_realloc_ul(
	_opaque_ void* ptr,
	alloc_t old_size,
	alloc_t new_size,
	int zero
	)
{
	const alloc_state_t* state = alloc_get_global_state();
	const alloc_handle_t* old_handle = alloc_get_handle(ptr, old_size);
	const alloc_handle_t* new_handle = alloc_get_handle_s(state, new_size);

	ALLOC_REALLOC(alloc_alloc_uh, alloc_free_uh);
}


void*
alloc_realloc_h(
	_opaque_ alloc_handle_t* old_handle,
	_opaque_ void* ptr,
	alloc_t old_size,
	_opaque_ alloc_handle_t* new_handle,
	alloc_t new_size,
	int zero
	)
{
	ALLOC_REALLOC(alloc_alloc_h, alloc_free_h);
}


void*
alloc_realloc_uh(
	_opaque_ alloc_handle_t* old_handle,
	_opaque_ void* ptr,
	alloc_t old_size,
	_opaque_ alloc_handle_t* new_handle,
	alloc_t new_size,
	int zero
	)
{
	ALLOC_REALLOC(alloc_alloc_uh, alloc_free_uh);
}


#undef ALLOC_REALLOC

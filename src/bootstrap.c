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
#include <alloc/sync.h>
#include <alloc/debug.h>
#include <alloc/macro.h>
#include <alloc/types.h>
#include <alloc/atomic.h>
#include <alloc/consts.h>
#include <alloc/platform.h>
#include <alloc/red_zone.h>
#include <alloc/valgrind.h>
#include <alloc/bootstrap.h>

#include <stdint.h>
#include <string.h>

#define alloc_bootstrap_log(verbosity, ...)	\
do											\
{											\
	++alloc_bootstrap.recursion;			\
	alloc_log_##verbosity (__VA_ARGS__);	\
	--alloc_bootstrap.recursion;			\
}											\
while(0)

#define alloc_bootstrap_log_debug(...)	\
alloc_bootstrap_log(debug, __VA_ARGS__)

#define alloc_bootstrap_log_info(...)	\
alloc_bootstrap_log(info, __VA_ARGS__)

#define alloc_bootstrap_log_warning(...)	\
alloc_bootstrap_log(warning, __VA_ARGS__)

#define alloc_bootstrap_log_error(...)	\
alloc_bootstrap_log(error, __VA_ARGS__)


typedef struct alloc_bootstrap_header
{
	/* Must be with sign for proper bit extension. */
	int32_t prev;
	int32_t next;

	uint32_t size;
}
alloc_bootstrap_header_t;


typedef struct alloc_bootstrap
{
	sync_mtx_t mtx;
	uint8_t initialized;
	uint32_t recursion;
	uint32_t allocated;

	void* ptr;
	void* end;
	void* head;

	struct
	{
		void* ptr;
		alloc_t offset;
		uint32_t allocated;
	}
	reserve;
}
alloc_bootstrap_t;


alloc_bootstrap_t alloc_bootstrap;


bool
alloc_is_bootstrap_ptr(
	const volatile void* ptr
	)
{
	return ptr >= alloc_bootstrap.ptr && ptr < alloc_bootstrap.end;
}


void
alloc_bootstrap_init(
	void
	)
{
	sync_mtx_init_recursive(&alloc_bootstrap.mtx);
}


void
alloc_bootstrap_lock(
	void
	)
{
	sync_mtx_lock(&alloc_bootstrap.mtx);
}


void
alloc_bootstrap_unlock(
	void
	)
{
	sync_mtx_unlock(&alloc_bootstrap.mtx);
}


void
alloc_bootstrap_reset(
	void
	)
{
	hard_assert_eq(alloc_bootstrap.allocated, 0);

	alloc_bootstrap_log_debug("bootstrap_free(): releasing memory");

	alloc_free_virtual(alloc_bootstrap.reserve.ptr, alloc_consts.bootstrap.size);
	alloc_bootstrap.ptr = NULL;

	alloc_bootstrap.head = NULL;
	alloc_bootstrap.reserve.ptr = NULL;
}


attr_alloc_fn void*
alloc_bootstrap_reserve_alloc(
	alloc_t size,
	alloc_t alignment,
	int zero
	)
{
	assert_not_null(alloc_bootstrap.reserve.ptr);

	void* ptr = alloc_bootstrap.reserve.ptr + alloc_bootstrap.reserve.offset;
	void* aligned_ptr = MACRO_ALIGN_UP(ptr, alignment - 1);

	alloc_t new_offset = aligned_ptr + size - alloc_bootstrap.reserve.ptr;
	if(attr_unlikely(new_offset > alloc_consts.bootstrap.reserve_size))
	{
		return NULL;
	}

	alloc_bootstrap.reserve.offset = new_offset;
	++alloc_bootstrap.reserve.allocated;

	if(zero)
	{
		memset(aligned_ptr, 0, size);
	}

	return aligned_ptr;
}


void
alloc_bootstrap_reserve_free(
	const volatile void* ptr,
	alloc_t size
	)
{
	hard_assert_neq(alloc_bootstrap.reserve.allocated--, 0);

	alloc_t ptr_offset = (void*) ptr - alloc_bootstrap.reserve.ptr;
	alloc_t end_offset = ptr_offset + size;

	if(end_offset == alloc_bootstrap.reserve.offset)
	{
		alloc_bootstrap.reserve.offset = ptr_offset;
	}

	if(!alloc_bootstrap.reserve.allocated)
	{
		alloc_bootstrap.reserve.offset = 0;
	}
}


void
alloc_bootstrap_check_reserve(
	void
	)
{
	if(alloc_bootstrap.recursion)
	{
		return;
	}

	if(!alloc_bootstrap.reserve.allocated)
	{
		return;
	}

	alloc_bootstrap_log_error("bootstrap_check_reserve(): leak detected, allocated=", alloc_bootstrap.reserve.allocated);
	hard_assert_unreachable();
}


#define alloc_bootstrap_enter(...)				\
alloc_bootstrap_lock();							\
ALLOC_VALGRIND_DISABLE_ERROR_REPORTING();		\
hard_assert_le(++alloc_bootstrap.recursion, 2)

#define alloc_bootstrap_leave(...)					\
hard_assert_neq(alloc_bootstrap.recursion--, 0);	\
alloc_bootstrap_check_reserve();					\
ALLOC_VALGRIND_ENABLE_ERROR_REPORTING();			\
alloc_bootstrap_unlock()


void*
alloc_bootstrap_calc_addr(
	alloc_bootstrap_header_t** header,
	alloc_t alignment
	)
{
	assert_eq((alloc_t) *header % alignof(alloc_bootstrap_header_t), 0);
	assert_neq(alignment, 0);
	assert_true(MACRO_IS_POWER_OF_2(alignment));

	void* data_addr = (void*)(*header + 1) + alloc_consts.red_zone.size;
	data_addr = MACRO_ALIGN_UP(data_addr, alignment - 1);
	data_addr -= alloc_consts.red_zone.size;

	*header = MACRO_ALIGN_DOWN(data_addr - sizeof(**header), alignof(alloc_bootstrap_header_t) - 1);
	return data_addr;
}


void
alloc_bootstrap_insert_header(
	alloc_bootstrap_header_t* header,
	alloc_bootstrap_header_t* split
	)
{
	uint32_t old_next = header->next;
	uint32_t shift = (void*) split - (void*) header;
	header->next = shift;

	split->prev = -shift;
	split->next = old_next ? old_next - shift : 0;
	split->size = 0;

	if(old_next)
	{
		alloc_bootstrap_header_t* next = (void*) header + old_next;
		next->prev = (void*) split - (void*) next;
	}
}


int
alloc_bootstrap_is_reserve_ptr(
	const volatile void* ptr
	)
{
	const volatile void* reserve = alloc_bootstrap.reserve.ptr;
	const volatile void* reserve_end = reserve + alloc_consts.bootstrap.reserve_size;
	return ptr >= reserve && ptr < reserve_end;
}


void
alloc_bootstrap_apply_red_zones(
	void* data_addr,
	alloc_t size
	)
{
	alloc_red_zone_init(data_addr);
	alloc_red_zone_init(data_addr + alloc_consts.red_zone.size + size);
}


attr_alloc_fn void*
alloc_bootstrap_alloc_u(
	alloc_t size,
	int zero
	)
{
	if(attr_unlikely(atomic_load_acq(&alloc_consts.state) < ALLOC_CONSTS_STATE_BOOTSTRAP))
	{
		return NULL;
	}

	if(attr_unlikely(!size))
	{
		return NULL;
	}

	alloc_t alignment = MACRO_NEXT_OR_EQUAL_POWER_OF_2(size);
	alignment = MACRO_MIN(alignment, alloc_consts.page.size);
	hard_assert_lt(size, INT32_MAX - alloc_consts.red_zone.size * 2 - sizeof(alloc_bootstrap_header_t) - alignment);

	if(attr_unlikely(size > alloc_consts.bootstrap.size))
	{
		alloc_bootstrap_log_warning("bootstrap_alloc(): suspicious size requested, size=", size, " alignment=", alignment);
		return NULL;
	}

	assert_neq(alloc_bootstrap.recursion, 0);
	if(alloc_bootstrap.recursion != 1)
	{
		return alloc_bootstrap_reserve_alloc(size, alignment, zero);
	}

	alloc_bootstrap_log_debug("bootstrap_alloc(): size=", size, " alignment=", alignment);

	if(!alloc_bootstrap.ptr)
	{
		void* vm = alloc_alloc_virtual(alloc_bootstrap.ptr, alloc_consts.bootstrap.size, -1, -1, 0);
		if(!vm)
		{
			return NULL;
		}

		alloc_bootstrap.reserve.ptr = vm;
		alloc_bootstrap.ptr = vm + alloc_consts.bootstrap.reserve_size;
		alloc_bootstrap.end = vm + alloc_consts.bootstrap.size;

		/* There must always be at least one header to simplify the logic.
		 * Furthermore, whenever possible, the last header should be free.
		 * This is so that if we don't find a suitable free block, the last
		 * one, if free, is used like a high-water mark. */

		alloc_bootstrap_header_t* free_header = alloc_bootstrap.ptr;
		free_header->prev = 0;
		free_header->next = 0;
		free_header->size = 0;

		alloc_bootstrap.head = free_header;
	}

	alloc_t data_size = size + alloc_consts.red_zone.size * 2;
	alloc_bootstrap_header_t* header = alloc_bootstrap.head;

	while(1)
	{
		if(header->size)
		{
			goto_next:

			if(!header->next)
			{
				break;
			}

			header = (void*) header + header->next;

			continue;
		}

		void* region_end = header->next ? (void*) header + header->next : alloc_bootstrap.end;

		alloc_bootstrap_header_t* new_header = header;
		void* data_addr = alloc_bootstrap_calc_addr(&new_header, alignment);

		if(data_addr + data_size > region_end)
		{
			goto goto_next;
		}

		int32_t old_prev = header->prev;
		int32_t old_next = header->next;
		alloc_t shift = (void*) new_header - (void*) header;

		if(old_prev)
		{
			alloc_bootstrap_header_t* prev = (void*) header + old_prev;
			prev->next += shift;
		}
		else
		{
			alloc_bootstrap.head = new_header;
		}

		if(old_next)
		{
			alloc_bootstrap_header_t* next = (void*) header + old_next;
			next->prev += shift;
		}

		new_header->prev = old_prev ? old_prev - shift : 0;
		new_header->next = old_next ? old_next - shift : 0;
		new_header->size = size;

		/* Can't insert a header before the new header, because the alignment of the previous data is unknown. */

		alloc_bootstrap_header_t* split = MACRO_ALIGN_UP(data_addr + data_size, alignof(alloc_bootstrap_header_t) - 1);
		if((void*)(split + 1) < region_end)
		{
			alloc_bootstrap_insert_header(new_header, split);
		}

		++alloc_bootstrap.allocated;

		alloc_bootstrap_apply_red_zones(data_addr, size);

		void* final_ptr = data_addr + alloc_consts.red_zone.size;
		if(zero)
		{
			memset(final_ptr, 0, size);
		}

		return final_ptr;
	}

	/* Out of memory in bootstrap must be an error, whereas in the normal allocator it doesn't really matter. */
	alloc_bootstrap_log_error("bootstrap_alloc(): OOM, size=", size, " alignment=", alignment);
	return NULL;
}


attr_alloc_fn void*
alloc_bootstrap_alloc(
	alloc_t size,
	int zero
	)
{
	alloc_bootstrap_enter();
		void* ptr = alloc_bootstrap_alloc_u(size, zero);
	alloc_bootstrap_leave();

	ALLOC_VALGRIND_ALLOC(ptr, size, zero);

	return ptr;
}


void
alloc_bootstrap_free_u(
	const volatile void* ptr,
	alloc_t size
	)
{
	if(attr_unlikely(atomic_load_acq(&alloc_consts.state) < ALLOC_CONSTS_STATE_BOOTSTRAP))
	{
		return;
	}

	assert_ptr(ptr, size);

	if(attr_unlikely(!ptr))
	{
		return;
	}

	if(alloc_bootstrap_is_reserve_ptr(ptr))
	{
		alloc_bootstrap_reserve_free(ptr, size);
		return;
	}

	assert_true(alloc_is_bootstrap_ptr(ptr));

	hard_assert_neq(alloc_bootstrap.allocated--, 0, alloc_bootstrap_log_error(
		"bootstrap_free(): double free or corruption, ptr=", ptr, " size=", size));

	alloc_bootstrap_log_debug("bootstrap_free(): ptr=", ptr, " size=", size);

	void* data_ptr = (void*) ptr - alloc_consts.red_zone.size;
	alloc_bootstrap_header_t* header =
		MACRO_ALIGN_DOWN(data_ptr - sizeof(*header), alignof(alloc_bootstrap_header_t) - 1);

	hard_assert_neq(header->size, 0, alloc_log_error(
		"bootstrap_free(): double free or corruption, ptr=", ptr, " size=", size));

	hard_assert_eq(header->size, size, alloc_log_error(
		"bootstrap_free(): size mismatch or corruption, ptr=",
		ptr, " size=", size, " header->size=", header->size));

#ifndef ALLOC_RELEASE
	alloc_red_zone_check(data_ptr, -1);
	alloc_red_zone_check((void*) ptr + size, 1);
#endif

	alloc_bootstrap_header_t* prev = header->prev ? (void*) header + header->prev : NULL;
	alloc_bootstrap_header_t* next = header->next ? (void*) header + header->next : NULL;

	if(!prev)
	{
		if(header != alloc_bootstrap.ptr)
		{
			/* Move back the header to the beginning of available space. */

			alloc_t shift = (void*) header - (void*) alloc_bootstrap.ptr;

			if(next)
			{
				next->prev -= shift;
			}

			uint32_t old_next = header->next;

			header = alloc_bootstrap.ptr;
			header->prev = 0;
			header->next = old_next + shift;
			header->size = 0;

			alloc_bootstrap.head = header;
		}
	}
	else if(!prev->size)
	{
		/* Merge with previous free block. */

		if(header->next)
		{
			prev->next += header->next;
		}
		else
		{
			prev->next = 0;
		}

		if(next)
		{
			next->prev = (void*) prev - (void*) next;
		}

		header = prev;
		prev = header->prev ? (void*) header + header->prev : NULL;
	}
	else
	{
		/* Mark as free. */
		header->size = 0;
	}

	if(!next)
	{
		/* Nothing to do here. */
	}
	else if(!next->size)
	{
		/* Merge with next free block. */

		if(!next->next)
		{
			/* Become the last block. */
			header->next = 0;
			next = NULL;
		}
		else
		{
			/* 2 free blocks can't exist in a row, so next_next must be allocated. */

			alloc_bootstrap_header_t* next_next = (void*) next + next->next;
			header->next = (void*) next_next - (void*) header;
			next_next->prev = (void*) header - (void*) next_next;

			next = next_next;
		}
	}
	else
	{
		/* Nothing to do here. */
	}

	if(!prev && !next)
	{
		/* Release memory. */
		alloc_bootstrap_reset();
	}
}


void
alloc_bootstrap_free(
	const volatile void* ptr,
	alloc_t size
	)
{
	alloc_bootstrap_enter();
		alloc_bootstrap_free_u(ptr, size);
	alloc_bootstrap_leave();

	ALLOC_VALGRIND_FREE(ptr);
}


attr_alloc_fn void*
alloc_bootstrap_realloc_u(
	const volatile void* ptr,
	alloc_t old_size,
	alloc_t new_size,
	int zero
	)
{
	if(attr_unlikely(atomic_load_acq(&alloc_consts.state) < ALLOC_CONSTS_STATE_BOOTSTRAP))
	{
		return NULL;
	}

	assert_ptr(ptr, old_size);

	if(attr_unlikely(!ptr))
	{
		return alloc_bootstrap_alloc_u(new_size, zero);
	}

	if(alloc_bootstrap_is_reserve_ptr(ptr))
	{
		void* new_ptr = alloc_bootstrap_alloc_u(new_size, zero);
		if(new_ptr)
		{
			memcpy(new_ptr, (void*) ptr, MACRO_MIN(old_size, new_size));
			alloc_bootstrap_free_u(ptr, old_size);
		}

		return new_ptr;
	}

	assert_true(alloc_is_bootstrap_ptr(ptr));

	if(attr_unlikely(!new_size))
	{
		alloc_bootstrap_free_u(ptr, old_size);
		return NULL;
	}

	alloc_t new_alignment = MACRO_NEXT_OR_EQUAL_POWER_OF_2(new_size);
	new_alignment = MACRO_MIN(new_alignment, alloc_consts.page.size);
	hard_assert_lt(new_size, INT32_MAX - alloc_consts.red_zone.size * 2 - sizeof(alloc_bootstrap_header_t) - new_alignment);

	if(attr_unlikely(new_size > alloc_consts.bootstrap.size))
	{
		alloc_bootstrap_log_warning(
			"bootstrap_realloc(): suspicious new_size requested, old_size=",
			old_size, " new_size=", new_size, " new_alignment=", new_alignment);
		return NULL;
	}

	alloc_bootstrap_log_debug("bootstrap_realloc(): ptr=", ptr,
		" old_size=", old_size, " new_size=", new_size, " alignment=", new_alignment);

	void* data_ptr = (void*) ptr - alloc_consts.red_zone.size;
	alloc_bootstrap_header_t* header =
		MACRO_ALIGN_DOWN(data_ptr - sizeof(*header), alignof(alloc_bootstrap_header_t) - 1);

	hard_assert_neq(header->size, 0, alloc_log_error(
		"bootstrap_realloc(): double free or corruption, ptr=", ptr, " old_size=",
		old_size, " new_size=", new_size, " new_alignment=", new_alignment));

	hard_assert_eq(header->size, old_size, alloc_log_error(
		"bootstrap_realloc(): size mismatch or corruption, ptr=",
		ptr, " old_size=", old_size, " header->size=", header->size));

#ifndef ALLOC_RELEASE
	alloc_red_zone_check(data_ptr, -1);
	alloc_red_zone_check((void*) ptr + old_size, 1);
#endif

	if((alloc_t) ptr % new_alignment != 0)
	{
		goto goto_alloc_new;
	}

	alloc_t data_size = new_size + alloc_consts.red_zone.size * 2;

	alloc_bootstrap_header_t* next = header->next ? (void*) header + header->next : NULL;

	void* region_end;
	if(next)
	{
		if(!next->size)
		{
			if(next->next)
			{
				region_end = (void*) next + next->next;
			}
			else
			{
				region_end = alloc_bootstrap.end;
			}
		}
		else
		{
			region_end = next;
		}
	}
	else
	{
		region_end = alloc_bootstrap.end;
	}

	alloc_bootstrap_header_t* split = MACRO_ALIGN_UP(data_ptr + data_size, alignof(alloc_bootstrap_header_t) - 1);
	if((void*)(split + 1) > region_end)
	{
		goto goto_alloc_new;
	}

	if(next && !next->size)
	{
		if(next->next)
		{
			alloc_bootstrap_header_t* next_next = (void*) next + next->next;
			next_next->prev -= header->next;
			header->next += next->next;
		}
		else
		{
			header->next = 0;
		}
	}

	header->size = new_size;

	if((void*)(split + 1) < region_end)
	{
		alloc_bootstrap_insert_header(header, split);
	}

	alloc_bootstrap_apply_red_zones(data_ptr, new_size);

	void* final_ptr = data_ptr + alloc_consts.red_zone.size;

	if(zero && new_size > old_size)
	{
		memset(final_ptr + old_size, 0, new_size - old_size);
	}

	return final_ptr;


	goto_alloc_new:

	void* new_ptr = alloc_bootstrap_alloc_u(new_size, zero);
	if(new_ptr)
	{
		memcpy(new_ptr, (void*) ptr, MACRO_MIN(old_size, new_size));
		alloc_bootstrap_free_u(ptr, old_size);
	}

	return new_ptr;
}


attr_alloc_fn void*
alloc_bootstrap_realloc(
	const volatile void* ptr,
	alloc_t old_size,
	alloc_t new_size,
	int zero
	)
{
	alloc_bootstrap_enter();
		void* new_ptr = alloc_bootstrap_realloc_u(ptr, old_size, new_size, zero);
	alloc_bootstrap_leave();

	if(ptr && ptr == new_ptr)
	{
		ALLOC_VALGRIND_RESIZE(ptr, old_size, new_size);
	}
	else if(new_ptr)
	{
		ALLOC_VALGRIND_ALLOC(new_ptr, new_size, zero);

		if(ptr)
		{
			ALLOC_VALGRIND_FREE(ptr);
		}
	}
	else if(!new_size && ptr)
	{
		ALLOC_VALGRIND_FREE(ptr);
	}

	return new_ptr;
}


#undef alloc_bootstrap_leave
#undef alloc_bootstrap_enter
#undef alloc_bootstrap_log_error
#undef alloc_bootstrap_log_warning
#undef alloc_bootstrap_log_info
#undef alloc_bootstrap_log_debug
#undef alloc_bootstrap_log

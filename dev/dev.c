#include "../src/debug.c"

#include <stdint.h>
#include <string.h>

#ifdef DEV_ALLOC
	#include "../include/alloc.h"


	void*
	dev_alloc(
		size_t size,
		int zero
		)
	{
		return alloc_alloc_h(
			alloc_get_handle_s(alloc_get_global_state(), size),
			size, zero);
	}


	void
	dev_free(
		const void* ptr,
		size_t size
		)
	{
		alloc_free_h(
			alloc_get_handle_s(alloc_get_global_state(), size),
			ptr, size);
	}


	void*
	dev_realloc(
		const void* ptr,
		size_t old_size,
		size_t new_size,
		int zero
		)
	{
		return allow_realloc_h(
			alloc_get_handle_s(alloc_get_global_state(), old_size),
			ptr, old_size,
			alloc_get_handle_s(alloc_get_global_state(), new_size),
			new_size, zero);
	}


#else
	#include <stdlib.h>


	void*
	dev_alloc(
		size_t size,
		int zero
		)
	{
		if(!zero)
		{
			return malloc(size);
		}

		return calloc(1, size);
	}


	void
	dev_free(
		const void* ptr,
		size_t size
		)
	{
		(void) size;

		free((void*) ptr);
	}


	void*
	dev_realloc(
		const void* ptr,
		size_t old_size,
		size_t new_size,
		int zero
		)
	{
		void* new_ptr = realloc((void*) ptr, new_size);
		if(!new_ptr)
		{
			return NULL;
		}

		if(new_size > old_size && zero)
		{
			(void) memset((uint8_t*) new_ptr + old_size, 0, new_size - old_size);
		}

		return new_ptr;
	}


#endif

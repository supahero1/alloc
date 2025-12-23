#include "../src/debug.c"

#include <stdint.h>
#include <string.h>

#ifdef DEV_ALLOC
	#include "../include/alloc_ext.h"

	void*
	dev_alloc(
		size_t size,
		int zero
		)
	{
		return alloc_alloc(NULL, size, zero);
	}


	void
	dev_free(
		const void* ptr,
		size_t size
		)
	{
		alloc_free(ptr, size);
	}


	void*
	dev_realloc(
		const void* ptr,
		size_t old_size,
		size_t new_size,
		int zero
		)
	{
		return alloc_realloc(ptr, old_size, new_size, zero);
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

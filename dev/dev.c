#include "../src/debug.c"

#include <stdint.h>
#include <string.h>

#ifdef DEV_ALLOC
	#include "../include/alloc_std.h"


	void*
	dev_alloc(
		size_t Size,
		int Zero
		)
	{
		return AllocAllocH(
			AllocGetHandleS(AllocGetGlobalState(), Size),
			Size, Zero);
	}


	void
	dev_free(
		const void* Ptr,
		size_t Size
		)
	{
		AllocFreeH(
			AllocGetHandleS(AllocGetGlobalState(), Size),
			Ptr, Size);
	}


	void*
	dev_realloc(
		const void* Ptr,
		size_t OldSize,
		size_t NewSize,
		int Zero
		)
	{
		return AllocReallocH(
			AllocGetHandleS(AllocGetGlobalState(), OldSize),
			Ptr, OldSize,
			AllocGetHandleS(AllocGetGlobalState(), NewSize),
			NewSize, Zero);
	}


#else
	#include <stdlib.h>


	void*
	dev_alloc(
		size_t Size,
		int Zero
		)
	{
		if(!Zero)
		{
			return malloc(Size);
		}

		return calloc(1, Size);
	}


	void
	dev_free(
		const void* Ptr,
		size_t Size
		)
	{
		(void) Size;

		free((void*) Ptr);
	}


	void*
	dev_realloc(
		const void* Ptr,
		size_t OldSize,
		size_t NewSize,
		int Zero
		)
	{
		void* NewPtr = realloc((void*) Ptr, NewSize);
		if(!NewPtr)
		{
			return NULL;
		}

		if(NewSize > OldSize && Zero)
		{
			(void) memset((uint8_t*) NewPtr + OldSize, 0, NewSize - OldSize);
		}

		return NewPtr;
	}


#endif


#pragma once

#include <stdint.h>


extern void*
Malloc(
	uint64_t Size
	);


extern void*
Calloc(
	uint64_t Size
	);


extern void
Free(
	void* Ptr,
	uint64_t Size
	);


extern void*
Remalloc(
	void* Ptr,
	uint64_t OldSize,
	uint64_t NewSize
	);


extern void*
Recalloc(
	void* Ptr,
	uint64_t OldSize,
	uint64_t NewSize
	);

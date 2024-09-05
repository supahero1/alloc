#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WIN32
	#if !defined(_GNU_SOURCE) && defined(_FEATURES_H)
		#error "alloc.c was included after features.h"
	#endif
	#define _GNU_SOURCE
#endif

#include "../include/alloc_std.h"
#include "../include/debug.h"

#include <assert.h>
#include <string.h>

#ifndef _packed_
	#define _packed_ __attribute__((packed))
#endif

#define ALLOC_IS_POWER_OF_2(X) (((X) & ((X) - 1)) == 0)

#define ALLOC_ARRAYLEN(X) (sizeof(X) / sizeof(*(X)))

#define ALLOC_MIN(X, Y)		\
({							\
	__typeof__(X) _X = (X);	\
	__typeof__(Y) _Y = (Y);	\
	_X < _Y ? _X : _Y;		\
})

#define ALLOC_MAX(X, Y)		\
({							\
	__typeof__(X) _X = (X);	\
	__typeof__(Y) _Y = (Y);	\
	_X > _Y ? _X : _Y;		\
})

#define ALLOC_ALIGN(Ptr, Mask) ((void*)(((alloc_t) (Ptr) + (Mask)) & ~(Mask)))


#ifdef _WIN32


	_alloc_func_ void*
	AllocAllocVirtual(
		alloc_t Size
		)
	{
		if(!Size)
		{
			return NULL;
		}

		return VirtualAlloc(NULL, Size,
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	}


	void
	AllocFreeVirtual(
		_opaque_ void* Ptr,
		alloc_t Size
		)
	{
		(void) Size;

		if(!Ptr)
		{
			return;
		}

		BOOL Status = VirtualFree((void*) Ptr, 0, MEM_RELEASE);
		AssertNEQ(Status, 0);
	}


	_alloc_func_ void*
	AllocAllocVirtualAligned(
		alloc_t Size,
		alloc_t Alignment,
		_out_ void** Ptr
		)
	{
		AssertGE(Alignment, 1);
		AssertEQ(ALLOC_IS_POWER_OF_2(Alignment), 1);

		if(!Size)
		{
			*Ptr = NULL;
			return NULL;
		}

		alloc_t Mask = Alignment - 1;
		alloc_t ActualSize = Size + Mask;

		void* RealPtr = VirtualAlloc(NULL,
			ActualSize, MEM_RESERVE, PAGE_NOACCESS);
		if(!RealPtr)
		{
			return NULL;
		}

		void* AlignedPtr = ALLOC_ALIGN(RealPtr, Mask);

		void* CommittedPtr = VirtualAlloc(
			AlignedPtr, Size, MEM_COMMIT, PAGE_READWRITE);
		if(!CommittedPtr)
		{
			AllocFreeVirtual(RealPtr, ActualSize);
			return NULL;
		}

		*Ptr = CommittedPtr;
		return RealPtr;
	}


	void
	AllocFreeVirtualAligned(
		_opaque_ void* Ptr,
		alloc_t Size,
		alloc_t Alignment
		)
	{
		AllocFreeVirtual(Ptr, Size + Alignment - 1);
	}


	_alloc_func_ void*
	AllocReallocVirtual(
		_opaque_ void* Ptr,
		alloc_t OldSize,
		alloc_t NewSize
		)
	{
		if(!NewSize)
		{
			AllocFreeVirtual(Ptr, OldSize);
			return NULL;
		}

		if(!Ptr)
		{
			return AllocAllocVirtual(NewSize);
		}

		void* NewPtr = AllocAllocVirtual(NewSize);
		if(!NewPtr)
		{
			return NULL;
		}

		alloc_t CopySize = ALLOC_MIN(OldSize, NewSize);
		memcpy(NewPtr, Ptr, CopySize);

		AllocFreeVirtual(Ptr, OldSize);

		return NewPtr;
	}


	_alloc_func_ void*
	AllocReallocVirtualAligned(
		_opaque_ void* RealPtr,
		alloc_t OldSize,
		alloc_t NewSize,
		alloc_t Alignment,
		_out_ void** NewPtr
		)
	{
		if(!NewSize)
		{
			AllocFreeVirtualAligned(RealPtr, OldSize, Alignment);
			*NewPtr = NULL;
			return NULL;
		}

		if(!RealPtr)
		{
			return AllocAllocVirtualAligned(NewSize, Alignment, NewPtr);
		}

		void* NewRealPtr = AllocAllocVirtualAligned(
			NewSize, Alignment, NewPtr);
		if(!NewRealPtr)
		{
			return NULL;
		}

		void* AlignedOldPtr = ALLOC_ALIGN(RealPtr, Alignment);
		void* AlignedNewPtr = *NewPtr;

		alloc_t CopySize = ALLOC_MIN(OldSize, NewSize);
		memcpy(AlignedNewPtr, AlignedOldPtr, CopySize);

		AllocFreeVirtualAligned(RealPtr, OldSize, Alignment);

		return NewRealPtr;
	}


#else
	#include <sys/mman.h>

	#ifndef MREMAP_MAYMOVE
		#define MREMAP_MAYMOVE 1
	#endif


	_alloc_func_ void*
	AllocAllocVirtual(
		alloc_t Size
		)
	{
		if(!Size)
		{
			return NULL;
		}

		void* Ptr = mmap(NULL, Size, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if(Ptr == MAP_FAILED)
		{
			return NULL;
		}

		return Ptr;
	}


	void
	AllocFreeVirtual(
		_opaque_ void* Ptr,
		alloc_t Size
		)
	{
		if(!Ptr)
		{
			return;
		}

		int Status = munmap((void*) Ptr, Size);
		AssertEQ(Status, 0);
	}


	_alloc_func_ void*
	AllocAllocVirtualAligned(
		alloc_t Size,
		alloc_t Alignment,
		_out_ void** Ptr
		)
	{
		AssertGE(Alignment, 1);
		AssertEQ(ALLOC_IS_POWER_OF_2(Alignment), 1);

		if(!Size)
		{
			*Ptr = NULL;
			return NULL;
		}

		alloc_t Mask = Alignment - 1;
		alloc_t ActualSize = Size + Mask;

		void* RealPtr = mmap(NULL, ActualSize, PROT_NONE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if(RealPtr == MAP_FAILED)
		{
			return NULL;
		}

		void* AlignedPtr = ALLOC_ALIGN(RealPtr, Mask);

		if(mprotect(AlignedPtr, Size, PROT_READ | PROT_WRITE))
		{
			AllocFreeVirtual(RealPtr, ActualSize);
			return NULL;
		}

		*Ptr = AlignedPtr;
		return RealPtr;
	}


	void
	AllocFreeVirtualAligned(
		_opaque_ void* RealPtr,
		alloc_t Size,
		alloc_t Alignment
		)
	{
		AllocFreeVirtual(RealPtr, Size + Alignment - 1);
	}


	_alloc_func_ void*
	AllocReallocVirtual(
		_opaque_ void* Ptr,
		alloc_t OldSize,
		alloc_t NewSize
		)
	{
		if(!NewSize)
		{
			AllocFreeVirtual(Ptr, OldSize);
			return NULL;
		}

		if(!Ptr)
		{
			return AllocAllocVirtual(NewSize);
		}

		void* NewPtr = mremap((void*) Ptr, OldSize, NewSize, MREMAP_MAYMOVE);
		if(NewPtr == MAP_FAILED)
		{
			return NULL;
		}

		return NewPtr;
	}


	_alloc_func_ void*
	AllocReallocVirtualAligned(
		_opaque_ void* RealPtr,
		alloc_t OldSize,
		alloc_t NewSize,
		alloc_t Alignment,
		_out_ void** NewPtr
		)
	{
		if(!NewSize)
		{
			AllocFreeVirtualAligned(RealPtr, OldSize, Alignment);
			*NewPtr = NULL;
			return NULL;
		}

		if(!RealPtr)
		{
			return AllocAllocVirtualAligned(NewSize, Alignment, NewPtr);
		}

		/* You know, in theory we could do a mremap here and check if the result
		 * is aligned properly, but that would work only if `NewSize` is double
		 * `OldSize` and overall the chances of a good outcome are slim, so do
		 * not bother.
		 */

		void* NewRealPtr = AllocAllocVirtualAligned(
			NewSize, Alignment, NewPtr);
		if(!NewRealPtr)
		{
			return NULL;
		}

		void* AlignedOldPtr = ALLOC_ALIGN(RealPtr, Alignment);
		void* AlignedNewPtr = *NewPtr;

		alloc_t CopySize = ALLOC_MIN(OldSize, NewSize);
		memcpy(AlignedNewPtr, AlignedOldPtr, CopySize);

		AllocFreeVirtualAligned(RealPtr, OldSize, Alignment);

		return NewRealPtr;
	}


	#include <unistd.h>
#endif


#if ALLOC_THREADS == 1
	#ifdef _WIN32


		Private void
		AllocMutexInit(
			AllocMutex* Mutex
			)
		{
			InitializeSRWLock(Mutex);
		}


		Private void
		AllocMutexDestroy(
			AllocMutex* Mutex
			)
		{
			(void) Mutex;
		}


		Private void
		AllocMutexLock(
			AllocMutex* Mutex
			)
		{
			AcquireSRWLockExclusive(Mutex);
		}


		Private void
		AllocMutexUnlock(
			AllocMutex* Mutex
			)
		{
			ReleaseSRWLockExclusive(Mutex);
		}


	#else


		Private void
		AllocMutexInit(
			AllocMutex* Mutex
			)
		{
			int Status = pthread_mutex_init(Mutex, NULL);
			AssertEQ(Status, 0);
		}


		Private void
		AllocMutexDestroy(
			AllocMutex* Mutex
			)
		{
			int Status = pthread_mutex_destroy(Mutex);
			AssertEQ(Status, 0);
		}


		Private void
		AllocMutexLock(
			AllocMutex* Mutex
			)
		{
			int Status = pthread_mutex_lock(Mutex);
			AssertEQ(Status, 0);
		}


		Private void
		AllocMutexUnlock(
			AllocMutex* Mutex
			)
		{
			int Status = pthread_mutex_unlock(Mutex);
			AssertEQ(Status, 0);
		}


	#endif

	#define ALLOC_LOCK(X) AllocMutexLock(X)
	#define ALLOC_UNLOCK(X) AllocMutexUnlock(X)
#else
	#define ALLOC_LOCK(X) ((void) X)
	#define ALLOC_UNLOCK(X) ((void) X)
#endif



/* PRIMITIVE ALLOCATORS
 */



#if __SIZEOF_POINTER__ == 8
	#define ALLOC1_MAX 250
#else
	#define ALLOC1_MAX 251
#endif


typedef struct _packed_ Alloc1 Alloc1;

struct _packed_ Alloc1
{
	uint8_t Next;
	uint8_t Used;
	uint8_t Count;
	uint8_t Free;
	uint8_t Data[ALLOC1_MAX];
};


typedef struct Alloc1Block Alloc1Block;

struct _packed_ Alloc1Block
{
	Alloc1Block* Prev;
	Alloc1Block* Next;
	void* RealPtr;
	uint16_t Count;
	uint16_t Free;
	Alloc1 Allocs[];
};

/* Optimized for the most common page size and default block size */
static_assert(sizeof(Alloc1Block) + sizeof(Alloc1) * 16 <= 4096,
	"Alloc1 size mismatch");


#define ALLOC2_MAX UINT16_MAX

typedef struct Alloc2 Alloc2;

struct _packed_ Alloc2
{
	Alloc2* Prev;
	Alloc2* Next;
	void* RealPtr;
	uint16_t Used;
	uint16_t Count;
	uint16_t Free;
};


#define ALLOC4_MAX UINT32_MAX

typedef struct Alloc4 Alloc4;

struct _packed_ Alloc4
{
	Alloc4* Prev;
	Alloc4* Next;
	void* RealPtr;
	uint32_t Used;
	uint32_t Count;
	uint32_t Free;
};


typedef struct AllocHandleInternal AllocHandleInternal;


typedef void*
(*AllocAllocFunc)(
	AllocHandleInternal* Handle,
	alloc_t Size,
	int Zero
	);


typedef void
(*AllocFreeFunc)(
	AllocHandleInternal* Handle,
	void* BlockPtr,
	void* Ptr,
	alloc_t Size
	);


typedef struct AllocHandleInternal
{
#if ALLOC_THREADS == 1
	AllocMutex Mutex;
#endif

	/* Padding for generic allocators (computed from `Alignment`).
	 */
	alloc_t Padding;
	alloc_t Allocators;
	alloc_t Allocations;
	alloc_t AllocLimit;
	alloc_t AllocSize;
	alloc_t BlockSize;

	AllocHandleFlag Flags;

	struct
	{
		void* Prev;
		void* Next;
		void* RealPtr;
	}
	*Head;

	AllocAllocFunc AllocFunc;
	AllocFreeFunc FreeFunc;
}
AllocHandleInternal;

static_assert(sizeof(AllocHandle) >= sizeof(AllocHandleInternal),
	"AllocHandle size mismatch");


#define ALLOC_PO2(X) (UINT32_C(1) << UINT32_C(X))
#define ALLOC_DEFAULT_BLOCK_SIZE ALLOC_PO2(23)

Private AllocHandleInfo AllocDefaultHandleInfo[] =
(AllocHandleInfo[])
{
	/* The defaults. For `Alloc1` the minimum size is set so that it gets
	 * clamped to the page size. For the rest 8MiB is set (clamped to at most
	 * `131072` for `Alloc2`). That is a reasonable tradeoff between memory
	 * fragmentation and performance. You can edit the macro above to suit you.
	 * Do not edit the code below.
	 */
/*   0*/{ .AllocSize = 1, .BlockSize =
			sizeof(Alloc1Block) + sizeof(Alloc1), .Alignment = 1 },
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(3)
/*   1*/{ .AllocSize = ALLOC_PO2(1), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(1) },
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(4)
/*   2*/{ .AllocSize = ALLOC_PO2(2), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(2)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(5)
/*   3*/{ .AllocSize = ALLOC_PO2(3), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(3)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(6)
/*   4*/{ .AllocSize = ALLOC_PO2(4), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(4)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(7)
/*   5*/{ .AllocSize = ALLOC_PO2(5), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(5)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(8)
/*   6*/{ .AllocSize = ALLOC_PO2(6), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(6)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(9)
/*   7*/{ .AllocSize = ALLOC_PO2(7), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(7)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(10)
/*   8*/{ .AllocSize = ALLOC_PO2(8), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(8)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(11)
/*   9*/{ .AllocSize = ALLOC_PO2(9), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(9)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(12)
/*  10*/{ .AllocSize = ALLOC_PO2(10), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(10)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(13)
/*  11*/{ .AllocSize = ALLOC_PO2(11), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(11)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(14)
/*  12*/{ .AllocSize = ALLOC_PO2(12), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(12)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(15)
/*  13*/{ .AllocSize = ALLOC_PO2(13), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(13)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(16)
/*  14*/{ .AllocSize = ALLOC_PO2(14), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(14)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(17)
/*  15*/{ .AllocSize = ALLOC_PO2(15), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(15)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(18)
/*  16*/{ .AllocSize = ALLOC_PO2(16), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(16)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(19)
/*  17*/{ .AllocSize = ALLOC_PO2(17), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(17)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(20)
/*  18*/{ .AllocSize = ALLOC_PO2(18), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(18)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(21)
/*  19*/{ .AllocSize = ALLOC_PO2(19), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(19)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(22)
/*  20*/{ .AllocSize = ALLOC_PO2(20), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(20)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(23)
/*  21*/{ .AllocSize = ALLOC_PO2(21), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(21)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(24)
/*  22*/{ .AllocSize = ALLOC_PO2(22), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(22)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(25)
/*  23*/{ .AllocSize = ALLOC_PO2(23), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(23)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(26)
/*  24*/{ .AllocSize = ALLOC_PO2(24), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(24)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(27)
/*  25*/{ .AllocSize = ALLOC_PO2(25), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(25)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(28)
/*  26*/{ .AllocSize = ALLOC_PO2(26), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(26)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(29)
/*  27*/{ .AllocSize = ALLOC_PO2(27), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(27)},
#endif
#if ALLOC_DEFAULT_BLOCK_SIZE >= ALLOC_PO2(30)
/*  28*/{ .AllocSize = ALLOC_PO2(28), .BlockSize =
			ALLOC_DEFAULT_BLOCK_SIZE, .Alignment = ALLOC_PO2(28)},
#endif
};

Private AllocStateInfo AllocDefaultStateInfo =
(AllocStateInfo)
{
	.Handles = AllocDefaultHandleInfo,
	.HandleCount = ALLOC_ARRAYLEN(AllocDefaultHandleInfo),
	.IndexFunc = NULL
};


Private alloc_t AllocPageSize;
Private alloc_t AllocPageSizeMask;
Private uint32_t AllocPageSizeShift;
Private const AllocState* AllocGlobalState;



/* FUNCTIONS
 */



Private uint32_t
AllocLog2(
	alloc_t Value
	)
{
	AssertNEQ(Value, 0);

	return __builtin_ctzll(Value);
}


Private uint32_t
AllocGetNextPO2(
	alloc_t Value
	)
{
	AssertNEQ(Value, 0);

	if(Value <= 2)
	{
		return Value;
	}

	return UINT32_C(1) << (32 - __builtin_clz(Value - 1));
}


Private __attribute__((constructor)) void
AllocLibraryInit(
	void
	)
{
#ifdef _WIN32
	SYSTEM_INFO Info;
	GetSystemInfo(&Info);
	AllocPageSize = dwPageSize;
#else
	AllocPageSize = getpagesize();
#endif

	AssertNEQ(AllocPageSize, 0);
	AssertEQ(ALLOC_IS_POWER_OF_2(AllocPageSize), 1);

	AllocPageSizeMask = AllocPageSize - 1;
	AllocPageSizeShift = AllocLog2(AllocPageSize);

#ifndef ALLOC_DO_NOT_AUTO_INIT_GLOBAL_STATE
	AllocGlobalState = AllocAllocState(NULL);
	AssertNEQ(AllocGlobalState, NULL);
#endif
}


Private __attribute__((destructor)) void
AllocLibraryDestroy(
	void
	)
{
#ifndef ALLOC_DO_NOT_AUTO_INIT_GLOBAL_STATE
	AllocFreeState(AllocGlobalState);
#endif
}


_const_func_ const AllocState*
AllocGetGlobalState(
	void
	)
{
	return AllocGlobalState;
}


_const_func_ alloc_t
AllocGetPageSize(
	void
	)
{
	return AllocPageSize;
}


Private void*
AllocAlloc1Func(
	AllocHandleInternal* Handle,
	alloc_t Size,
	int Zero
	)
{
	(void) Size;

	Alloc1Block* Block = (void*) Handle->Head;
	if(!Block)
	{
		void* RealPtr = AllocAllocVirtualAligned(
			Handle->BlockSize, Handle->BlockSize, (void**) &Block);
		if(!RealPtr)
		{
			return NULL;
		}

		/*
		Block->Prev = NULL;
		Block->Next = NULL;
		*/
		Block->RealPtr = RealPtr;
		/*
		Block->Count = 0;
		Block->Free = 0;
		*/

		alloc_t i = 0;
		Alloc1* Alloc = Block->Allocs;

		for(; i < Handle->AllocLimit - 1; ++i, ++Alloc)
		{
			Alloc->Next = i + 1;
			/*
			Alloc->Used = 0;
			Alloc->Count = 0;
			*/
			Alloc->Free = UINT8_MAX;
		}

		Alloc->Next = UINT8_MAX;
		/*
		Alloc->Used = 0;
		Alloc->Count = 0;
		*/
		Alloc->Free = UINT8_MAX;

		++Handle->Allocators;
		Handle->Head = (void*) Block;
	}

	Alloc1* Alloc = &Block->Allocs[Block->Free];

	++Handle->Allocations;
	++Block->Count;
	++Alloc->Count;

	if(Alloc->Count == ALLOC1_MAX)
	{
		if(Block->Count == ALLOC1_MAX * Handle->AllocLimit)
		{
			Handle->Head = (void*) Block->Next;

			if(Block->Next)
			{
				Block->Next->Prev = NULL;
			}

			Block->Prev = NULL;
			Block->Next = NULL;
		}
		else
		{
			Block->Free = Alloc->Next;
		}
	}

	if(Alloc->Free != UINT8_MAX)
	{
		uint8_t* Ptr = Alloc->Data + Alloc->Free;
		Alloc->Free = *Ptr;

		if(Zero)
		{
			*Ptr = 0;
		}

		return Ptr;
	}

	return Alloc->Data + Alloc->Used++;
}


Private void
AllocFree1Func(
	AllocHandleInternal* Handle,
	void* BlockPtr,
	void* Ptr,
	alloc_t Size
	)
{
	(void) Size;

	Alloc1Block* Block = BlockPtr;
	Alloc1* Alloc = &Block->Allocs[
		((uintptr_t) Ptr - (uintptr_t) Block - sizeof(Alloc1Block))
		/ sizeof(Alloc1)];

	--Handle->Allocations;
	--Block->Count;
	--Alloc->Count;

	if(
		Block->Count == 0 &&
		(
			(Handle->Flags & ALLOC_HANDLE_FLAG_IMMEDIATE_FREE) ||
			(
				Handle->Allocators >= 2 &&
				!(Handle->Flags & ALLOC_HANDLE_FLAG_DO_NOT_FREE) &&
				Handle->Allocations <= ALLOC1_MAX *
					Handle->AllocLimit * (Handle->Allocators - 2)
			)
		)
		)
	{
		if(Block->Prev)
		{
			Block->Prev->Next = Block->Next;
		}
		else
		{
			Handle->Head = (void*) Block->Next;
		}

		if(Block->Next)
		{
			Block->Next->Prev = Block->Prev;
		}

		AllocFreeVirtualAligned(Block->RealPtr,
			Handle->BlockSize, Handle->BlockSize);

		--Handle->Allocators;
	}
	else
	{
		if(Alloc->Count == ALLOC1_MAX - 1)
		{
			Alloc->Next = Block->Free;
			Block->Free = Alloc - Block->Allocs;

			if(Block->Count == ALLOC1_MAX * Handle->AllocLimit - 1)
			{
				if(Handle->Head)
				{
					Handle->Head->Prev = Block;
				}

				AssertEQ(Block->Prev, NULL);
				Block->Next = (void*) Handle->Head;
				Handle->Head = (void*) Block;
			}
		}


		*((uint8_t*) Ptr) = Alloc->Free;
		Alloc->Free = (uint8_t*) Ptr - Alloc->Data;
	}
}


Private void*
AllocAlloc2Func(
	AllocHandleInternal* Handle,
	alloc_t Size,
	int Zero
	)
{
	(void) Size;

	Alloc2* Alloc = (void*) Handle->Head;
	if(!Alloc)
	{
		void* RealPtr = AllocAllocVirtualAligned(
			Handle->BlockSize, Handle->BlockSize, (void**) &Alloc);
		if(!RealPtr)
		{
			return NULL;
		}

		Alloc->RealPtr = RealPtr;
		Alloc->Free = ALLOC2_MAX;

		++Handle->Allocators;
		Handle->Head = (void*) Alloc;
	}

	++Handle->Allocations;
	++Alloc->Count;

	uint8_t* Data = (uint8_t*) Alloc + Handle->Padding;

	if(Alloc->Count == Handle->AllocLimit)
	{
		Handle->Head = (void*) Alloc->Next;

		if(Alloc->Next)
		{
			Alloc->Next->Prev = NULL;
		}

		Alloc->Next = NULL;
	}

	if(Alloc->Free != ALLOC2_MAX)
	{
		void* Ptr = Data + Alloc->Free * 2;

		(void) memcpy(&Alloc->Free, Ptr, 2);

		if(Zero)
		{
			(void) memset(Ptr, 0, 2);
		}

		return Ptr;
	}

	return Data + Alloc->Used++ * 2;
}


Private void
AllocFree2Func(
	AllocHandleInternal* Handle,
	void* BlockPtr,
	void* Ptr,
	alloc_t Size
	)
{
	(void) Size;

	Alloc2* Alloc = BlockPtr;

	--Handle->Allocations;
	--Alloc->Count;

	if(
		Alloc->Count == 0 &&
		(
			(Handle->Flags & ALLOC_HANDLE_FLAG_IMMEDIATE_FREE) ||
			(
				Handle->Allocators >= 2 &&
				!(Handle->Flags & ALLOC_HANDLE_FLAG_DO_NOT_FREE) &&
				Handle->Allocations <=
					Handle->AllocLimit * (Handle->Allocators - 2)
			)
		)
		)
	{
		if(Alloc->Prev)
		{
			Alloc->Prev->Next = Alloc->Next;
		}
		else
		{
			Handle->Head = (void*) Alloc->Next;
		}

		if(Alloc->Next)
		{
			Alloc->Next->Prev = Alloc->Prev;
		}

		AllocFreeVirtualAligned(Alloc->RealPtr,
			Handle->BlockSize, Handle->BlockSize);

		--Handle->Allocators;
	}
	else
	{
		if(Alloc->Count == Handle->AllocLimit - 1)
		{
			if(Handle->Head)
			{
				Handle->Head->Prev = Alloc;
			}

			AssertEQ(Alloc->Prev, NULL);
			Alloc->Next = (void*) Handle->Head;
			Handle->Head = (void*) Alloc;
		}


		(void) memcpy(Ptr, &Alloc->Free, 2);

		uint8_t* Data = (uint8_t*) Alloc + Handle->Padding;
		Alloc->Free = ((uintptr_t) Ptr - (uintptr_t) Data) / 2;
	}
}


Private void*
AllocAlloc4Func(
	AllocHandleInternal* Handle,
	alloc_t Size,
	int Zero
	)
{
	(void) Size;

	Alloc4* Alloc = (void*) Handle->Head;
	if(!Alloc)
	{
		void* RealPtr = AllocAllocVirtualAligned(
			Handle->BlockSize, Handle->BlockSize, (void**) &Alloc);
		if(!RealPtr)
		{
			return NULL;
		}

		Alloc->RealPtr = RealPtr;
		Alloc->Free = ALLOC4_MAX;

		++Handle->Allocators;
		Handle->Head = (void*) Alloc;
	}

	++Handle->Allocations;
	++Alloc->Count;

	uint8_t* Data = (uint8_t*) Alloc + Handle->Padding;

	if(Alloc->Count == Handle->AllocLimit)
	{
		Handle->Head = (void*) Alloc->Next;

		if(Alloc->Next)
		{
			Alloc->Next->Prev = NULL;
		}

		Alloc->Next = NULL;
	}

	if(Alloc->Free != ALLOC4_MAX)
	{
		void* Ptr = Data + Alloc->Free * Handle->AllocSize;

		(void) memcpy(&Alloc->Free, Ptr, 4);

		if(Zero)
		{
			(void) memset(Ptr, 0, Handle->AllocSize);
		}

		return Ptr;
	}

	return Data + Alloc->Used++ * Handle->AllocSize;
}


Private void
AllocFree4Func(
	AllocHandleInternal* Handle,
	void* BlockPtr,
	void* Ptr,
	alloc_t Size
	)
{
	(void) Size;

	Alloc4* Alloc = BlockPtr;

	--Handle->Allocations;
	--Alloc->Count;

	if(
		Alloc->Count == 0 &&
		(
			(Handle->Flags & ALLOC_HANDLE_FLAG_IMMEDIATE_FREE) ||
			(
				Handle->Allocators >= 2 &&
				!(Handle->Flags & ALLOC_HANDLE_FLAG_DO_NOT_FREE) &&
				Handle->Allocations <=
					Handle->AllocLimit * (Handle->Allocators - 2)
			)
		)
		)
	{
		if(Alloc->Prev)
		{
			Alloc->Prev->Next = Alloc->Next;
		}
		else
		{
			Handle->Head = (void*) Alloc->Next;
		}

		if(Alloc->Next)
		{
			Alloc->Next->Prev = Alloc->Prev;
		}

		AllocFreeVirtualAligned(Alloc->RealPtr,
			Handle->BlockSize, Handle->BlockSize);

		--Handle->Allocators;
	}
	else
	{
		if(Alloc->Count == Handle->AllocLimit - 1)
		{
			if(Handle->Head)
			{
				Handle->Head->Prev = Alloc;
			}

			AssertEQ(Alloc->Prev, NULL);
			Alloc->Next = (void*) Handle->Head;
			Handle->Head = (void*) Alloc;
		}


		(void) memcpy(Ptr, &Alloc->Free, 4);

		uint8_t* Data = (uint8_t*) Alloc + Handle->Padding;
		Alloc->Free = ((uintptr_t) Ptr - (uintptr_t) Data) / Handle->AllocSize;
	}
}


Private void*
AllocAllocVirtualFunc(
	AllocHandleInternal* Handle,
	alloc_t Size,
	int Zero
	)
{
	(void) Handle;
	(void) Zero;

	return AllocAllocVirtual(Size);
}


Private void
AllocFreeVirtualFunc(
	AllocHandleInternal* Handle,
	void* BlockPtr,
	void* Ptr,
	alloc_t Size
	)
{
	(void) Handle;

	AssertEQ(BlockPtr, Ptr);

	AllocFreeVirtual(Ptr, Size);
}


Private int
AllocHandleIsVirtual(
	_in_ AllocHandleInternal* Handle
	)
{
	return !Handle->AllocLimit;
}


void
AllocCreateHandle(
	_in_ AllocHandleInfo* Info,
	_opaque_ AllocHandle* Handle
	)
{
	AllocHandleInternal* HandleInternal = (AllocHandleInternal*) Handle;

#if ALLOC_THREADS == 1
	AllocMutexInit(&HandleInternal->Mutex);
#endif

	HandleInternal->Allocators = 0;
	HandleInternal->Allocations = 0;

	HandleInternal->Head = NULL;

	HandleInternal->Flags = ALLOC_HANDLE_FLAG_NONE;


	if(!Info)
	{
		HandleInternal->Padding = 0;
		HandleInternal->AllocLimit = 0;
		HandleInternal->AllocSize = 0;
		HandleInternal->BlockSize = 0;

		HandleInternal->AllocFunc = AllocAllocVirtualFunc;
		HandleInternal->FreeFunc = AllocFreeVirtualFunc;

		return;
	}


	AssertNEQ(Info->Alignment, 0);
	AssertEQ(ALLOC_IS_POWER_OF_2(Info->Alignment), 1);


	static const alloc_t BlockSizeMax[] =
	(const alloc_t[])
	{
		0,
		65536,
		131072,
		1073741824
	};

	static const alloc_t AllocLimitMax[] =
	(const alloc_t[])
	{
		0,
		UINT8_MAX - 2,
		UINT16_MAX - 2,
		UINT32_MAX - 2
	};

	static const AllocAllocFunc AllocFuncs[] =
	(const AllocAllocFunc[])
	{
		NULL,
		AllocAlloc1Func,
		AllocAlloc2Func,
		AllocAlloc4Func
	};

	static const AllocFreeFunc FreeFuncs[] =
	(const AllocFreeFunc[])
	{
		NULL,
		AllocFree1Func,
		AllocFree2Func,
		AllocFree4Func
	};

	alloc_t TableIndex = ALLOC_MIN(Info->AllocSize, 3U);


	if(Info->AllocSize == 1)
	{
		alloc_t BlockSize = Info->BlockSize;
		BlockSize = ALLOC_MIN(BlockSize, BlockSizeMax[TableIndex]);
		BlockSize = ALLOC_MAX(BlockSize, AllocPageSize);
		BlockSize = AllocGetNextPO2(BlockSize);

		alloc_t AllocLimit =
			(BlockSize - sizeof(Alloc1Block)) / sizeof(Alloc1);
		AllocLimit = ALLOC_MIN(AllocLimit, AllocLimitMax[TableIndex]);
		AllocLimit = ALLOC_MAX(AllocLimit, 1U);

		BlockSize = sizeof(Alloc1Block) + AllocLimit * sizeof(Alloc1);
		BlockSize = AllocGetNextPO2(BlockSize);

		HandleInternal->Padding = 0;
		HandleInternal->AllocLimit = AllocLimit;
		HandleInternal->AllocSize = 1;
		HandleInternal->BlockSize = BlockSize;

		HandleInternal->AllocFunc = AllocFuncs[TableIndex];
		HandleInternal->FreeFunc = FreeFuncs[TableIndex];

		return;
	}


	alloc_t AllocSize = Info->AllocSize == 2 ? sizeof(Alloc2) : sizeof(Alloc4);

	alloc_t Mask = Info->Alignment - 1;
	alloc_t Padding = (AllocSize + Mask) & ~Mask;

	alloc_t BlockSize = Info->BlockSize;
	BlockSize = ALLOC_MIN(BlockSize, BlockSizeMax[TableIndex]);
	BlockSize = ALLOC_MAX(BlockSize, AllocPageSize);
	BlockSize = AllocGetNextPO2(BlockSize);

	alloc_t AllocLimit = (BlockSize - AllocSize) / Info->AllocSize;
	AllocLimit = ALLOC_MIN(AllocLimit, AllocLimitMax[TableIndex]);
	AllocLimit = ALLOC_MAX(AllocLimit, 1U);

	BlockSize = Padding + AllocLimit * Info->AllocSize;
	BlockSize = AllocGetNextPO2(BlockSize);

	HandleInternal->Padding = Padding;
	HandleInternal->AllocLimit = AllocLimit;
	HandleInternal->AllocSize = Info->AllocSize;
	HandleInternal->BlockSize = Info->BlockSize;

	HandleInternal->AllocFunc = AllocFuncs[TableIndex];
	HandleInternal->FreeFunc = FreeFuncs[TableIndex];
}


void
AllocDestroyHandle(
	_opaque_ AllocHandle* Handle
	)
{
	AllocHandleInternal* HandleInternal = (AllocHandleInternal*) Handle;

	if(HandleInternal->Head)
	{
		AllocFreeVirtualAligned(HandleInternal->Head->RealPtr,
			HandleInternal->BlockSize, HandleInternal->BlockSize);
	}

#if ALLOC_THREADS == 1
	AllocMutexDestroy(&HandleInternal->Mutex);
#endif
}


Private uint32_t
AllocDefaultIndexFunc(
	alloc_t Size
	)
{
	return AllocLog2(AllocGetNextPO2(Size));
}


_alloc_func_ const AllocState*
AllocAllocState(
	_in_ AllocStateInfo* Info
	)
{
	if(!Info)
	{
		Info = &AllocDefaultStateInfo;
	}


	alloc_t HandleCount = Info->HandleCount + 1;
	AllocState* State = AllocAllocVirtual(
		sizeof(AllocState) + sizeof(AllocHandle) * HandleCount);
	if(!State)
	{
		return NULL;
	}

	if(!Info->IndexFunc)
	{
		State->IndexFunc = AllocDefaultIndexFunc;
	}
	else
	{
		State->IndexFunc = Info->IndexFunc;
	}

	State->HandleCount = HandleCount;


	AllocHandleInfo* HandleInfo = Info->Handles;
	AllocHandleInfo* HandleInfoEnd = HandleInfo + Info->HandleCount;

	alloc_t i = 0;

	for(; HandleInfo < HandleInfoEnd; ++HandleInfo, ++i)
	{
		AllocCreateHandle(HandleInfo, &State->Handles[i]);
	}

	AllocCreateHandle(NULL, &State->Handles[i]);


	return State;
}


void
AllocFreeState(
	_opaque_ AllocState* State
	)
{
	if(!State)
	{
		State = AllocGlobalState;
	}


	alloc_t i = 0;
	alloc_t HandleCount = State->HandleCount;

	for(; i < HandleCount; ++i)
	{
		AllocDestroyHandle(&State->Handles[i]);
	}

	AllocFreeVirtual(State, sizeof(AllocState) +
		HandleCount * sizeof(AllocHandleInternal));
}


_pure_func_ _opaque_ AllocHandle*
AllocGetHandleS(
	_in_ AllocState* State,
	alloc_t Size
	)
{
	AssertNEQ(Size, 0);

	uint32_t Index = State->IndexFunc(Size);
	Index = ALLOC_MIN(Index, State->HandleCount - 1);

	return &State->Handles[Index];
}


void
AllocHandleLockH(
	_opaque_ AllocHandle* Handle
	)
{
	AllocHandleInternal* HandleInternal = (AllocHandleInternal*) Handle;

	ALLOC_LOCK(&HandleInternal->Mutex);
}


void
AllocHandleUnlockH(
	_opaque_ AllocHandle* Handle
	)
{
	AllocHandleInternal* HandleInternal = (AllocHandleInternal*) Handle;

	ALLOC_UNLOCK(&HandleInternal->Mutex);
}


void
AllocHandleSetFlagsH(
	_opaque_ AllocHandle* Handle,
	AllocHandleFlag Flags
	)
{
	AllocHandleLockH(Handle);
		AllocHandleSetFlagsUH(Handle, Flags);
	AllocHandleUnlockH(Handle);
}


void
AllocHandleSetFlagsUH(
	_opaque_ AllocHandle* Handle,
	AllocHandleFlag Flags
	)
{
	AllocHandleInternal* HandleInternal = (AllocHandleInternal*) Handle;

	HandleInternal->Flags = Flags;
}


void
AllocHandleAddFlagsH(
	_opaque_ AllocHandle* Handle,
	AllocHandleFlag Flags
	)
{
	AllocHandleLockH(Handle);
		AllocHandleAddFlagsUH(Handle, Flags);
	AllocHandleUnlockH(Handle);
}


void
AllocHandleAddFlagsUH(
	_opaque_ AllocHandle* Handle,
	AllocHandleFlag Flags
	)
{
	AllocHandleInternal* HandleInternal = (AllocHandleInternal*) Handle;

	HandleInternal->Flags |= Flags;
}


void
AllocHandleDelFlagsH(
	_opaque_ AllocHandle* Handle,
	AllocHandleFlag Flags
	)
{
	AllocHandleLockH(Handle);
		AllocHandleDelFlagsUH(Handle, Flags);
	AllocHandleUnlockH(Handle);
}


void
AllocHandleDelFlagsUH(
	_opaque_ AllocHandle* Handle,
	AllocHandleFlag Flags
	)
{
	AllocHandleInternal* HandleInternal = (AllocHandleInternal*) Handle;

	HandleInternal->Flags &= ~Flags;
}


AllocHandleFlag
AllocHandleGetFlagsH(
	_opaque_ AllocHandle* Handle
	)
{
	AllocHandleFlag Flags;

	AllocHandleLockH(Handle);
		Flags = AllocHandleGetFlagsUH(Handle);
	AllocHandleUnlockH(Handle);

	return Flags;
}


AllocHandleFlag
AllocHandleGetFlagsUH(
	_opaque_ AllocHandle* Handle
	)
{
	AllocHandleInternal* HandleInternal = (AllocHandleInternal*) Handle;

	return HandleInternal->Flags;
}


Private void*
GetBasePtr(
	AllocHandleInternal* Handle,
	_in_ void* Ptr
	)
{
	if(AllocHandleIsVirtual(Handle))
	{
		return (void*) Ptr;
	}

	void* BlockPtr = (void*)
		((uintptr_t) Ptr & ~(Handle->BlockSize - 1));

	return BlockPtr;
}


_alloc_func_ void*
AllocAllocH(
	_opaque_ AllocHandle* Handle,
	alloc_t Size,
	int Zero
	)
{
	void* Ptr;

	AllocHandleLockH(Handle);
		Ptr = AllocAllocUH(Handle, Size, Zero);
	AllocHandleUnlockH(Handle);

	return Ptr;
}


_alloc_func_ void*
AllocAllocUH(
	_opaque_ AllocHandle* Handle,
	alloc_t Size,
	int Zero
	)
{
	AllocHandleInternal* HandleInternal = (AllocHandleInternal*) Handle;

	return HandleInternal->AllocFunc(HandleInternal, Size, Zero);
}


void
AllocFreeH(
	_opaque_ AllocHandle* Handle,
	_opaque_ void* Ptr,
	alloc_t Size
	)
{
	AllocHandleLockH(Handle);
		AllocFreeUH(Handle, Ptr, Size);
	AllocHandleUnlockH(Handle);
}


void
AllocFreeUH(
	_opaque_ AllocHandle* Handle,
	_opaque_ void* Ptr,
	alloc_t Size
	)
{
	AllocHandleInternal* HandleInternal = (void*) Handle;

	HandleInternal->FreeFunc(HandleInternal,
		GetBasePtr(HandleInternal, Ptr), (void*) Ptr, Size);
}


void*
AllocReallocH(
	_opaque_ AllocHandle* OldHandle,
	_opaque_ void* Ptr,
	alloc_t OldSize,
	_opaque_ AllocHandle* NewHandle,
	alloc_t NewSize,
	int Zero
	)
{
	if(!NewSize)
	{
		AllocFreeH(OldHandle, Ptr, OldSize);
		return NULL;
	}

	if(!Ptr)
	{
		return AllocAllocH(NewHandle, NewSize, Zero);
	}

	if(OldHandle == NewHandle)
	{
		if(AllocHandleIsVirtual((void*) OldHandle))
		{
			return AllocReallocVirtual(Ptr, OldSize, NewSize);
		}

		if(NewSize > OldSize && Zero)
		{
			(void) memset((uint8_t*) Ptr + OldSize, 0, NewSize - OldSize);
		}

		return (void*) Ptr;
	}

	void* NewPtr = AllocAllocH(NewHandle, NewSize, Zero);
	if(!NewPtr)
	{
		return NULL;
	}

	(void) memcpy(NewPtr, Ptr, ALLOC_MIN(OldSize, NewSize));

	AllocFreeH(OldHandle, Ptr, OldSize);

	return NewPtr;
}


void*
AllocReallocUH(
	_opaque_ AllocHandle* OldHandle,
	_opaque_ void* Ptr,
	alloc_t OldSize,
	_opaque_ AllocHandle* NewHandle,
	alloc_t NewSize,
	int Zero
	)
{
	if(!NewSize)
	{
		AllocFreeUH(OldHandle, Ptr, OldSize);
		return NULL;
	}

	if(!Ptr)
	{
		return AllocAllocUH(NewHandle, NewSize, Zero);
	}

	if(OldHandle == NewHandle)
	{
		if(AllocHandleIsVirtual((void*) OldHandle))
		{
			return AllocReallocVirtual(Ptr, OldSize, NewSize);
		}

		if(NewSize > OldSize && Zero)
		{
			(void) memset((uint8_t*) Ptr + OldSize, 0, NewSize - OldSize);
		}

		return (void*) Ptr;
	}

	void* NewPtr = AllocAllocUH(NewHandle, NewSize, Zero);
	if(!NewPtr)
	{
		return NULL;
	}

	(void) memcpy(NewPtr, Ptr, ALLOC_MIN(OldSize, NewSize));

	AllocFreeUH(OldHandle, Ptr, OldSize);

	return NewPtr;
}


#ifdef __cplusplus
}
#endif

#include "alloc.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>

#ifndef ALLOC_THREADS
	#define ALLOC_THREADS
#endif

#ifdef ALLOC_THREADS
	#include <pthread.h>
#endif


typedef struct Alloc1 Alloc1;

struct Alloc1
{
	uint8_t Next;
	uint8_t Used;
	uint8_t Count;
	uint8_t Free;
	uint8_t Data[250];
};


typedef struct Alloc1Block Alloc1Block;

struct Alloc1Block
{
	Alloc1Block* Prev;
	Alloc1Block* Next;
	uint32_t Count;
	uint32_t Free;
	uint32_t Padding[2];
	Alloc1 Allocs[16];
};


typedef struct Alloc2 Alloc2;

#ifndef ALLOC2_MAX
	#define ALLOC2_MAX UINT32_C(65521)
	#define ALLOC2_EXPECTED_SIZE 0x20000
#endif

#define ALLOC2_SIZE sizeof(Alloc2)
#define ALLOC2_MASK (ALLOC2_SIZE - 1)

struct Alloc2
{
	Alloc2* Prev;
	Alloc2* Next;
	void* RealPtr;
	uint16_t Used;
	uint16_t Count;
	uint16_t Free;
	uint16_t Data[ALLOC2_MAX];
};

static_assert(ALLOC2_SIZE == ALLOC2_EXPECTED_SIZE, "Alloc2 size mismatch");


typedef struct Alloc4 Alloc4;

#ifndef ALLOC4_MAX
	#define ALLOC4_MAX UINT32_C(262135)
	#define ALLOC4_EXPECTED_SIZE 0x100000
#endif

#define ALLOC4_SIZE sizeof(Alloc4)
#define ALLOC4_MASK (ALLOC4_SIZE - 1)

typedef struct Data4
{
	uint32_t Data[1];
}
Data4;

struct Alloc4
{
	Alloc4* Prev;
	Alloc4* Next;
	void* RealPtr;
	uint32_t Used;
	uint32_t Count;
	uint32_t Free;
	Data4 Data[ALLOC4_MAX];
};

static_assert(ALLOC4_SIZE == ALLOC4_EXPECTED_SIZE, "Alloc4 size mismatch");


typedef struct Alloc8 Alloc8;

#ifndef ALLOC8_MAX
	#define ALLOC8_MAX UINT32_C(131067)
	#define ALLOC8_EXPECTED_SIZE 0x100000
#endif

#define ALLOC8_SIZE sizeof(Alloc8)
#define ALLOC8_MASK (ALLOC8_SIZE - 1)

typedef struct Data8
{
	uint32_t Data[2];
}
Data8;

struct Alloc8
{
	Alloc8* Prev;
	Alloc8* Next;
	void* RealPtr;
	uint32_t Used;
	uint32_t Count;
	uint32_t Free;
	uint32_t Padding;
	Data8 Data[ALLOC8_MAX];
};

static_assert(ALLOC8_SIZE == ALLOC8_EXPECTED_SIZE, "Alloc8 size mismatch");


typedef struct Alloc16 Alloc16;

#ifndef ALLOC16_MAX
	#define ALLOC16_MAX UINT32_C(65533)
	#define ALLOC16_EXPECTED_SIZE 0x100000
#endif

#define ALLOC16_SIZE sizeof(Alloc16)
#define ALLOC16_MASK (ALLOC16_SIZE - 1)

typedef struct Data16
{
	uint32_t Data[4];
}
Data16;

struct Alloc16
{
	Alloc16* Prev;
	Alloc16* Next;
	void* RealPtr;
	uint32_t Used;
	uint32_t Count;
	uint32_t Free;
	uint32_t Padding[3];
	Data16 Data[ALLOC16_MAX];
};

static_assert(ALLOC16_SIZE == ALLOC16_EXPECTED_SIZE, "Alloc16 size mismatch");


typedef struct Alloc32 Alloc32;

#ifndef ALLOC32_MAX
	#define ALLOC32_MAX UINT32_C(32766)
	#define ALLOC32_EXPECTED_SIZE 0x100000
#endif

#define ALLOC32_SIZE sizeof(Alloc32)
#define ALLOC32_MASK (ALLOC32_SIZE - 1)

typedef struct Data32
{
	uint32_t Data[8];
}
Data32;

struct Alloc32
{
	Alloc32* Prev;
	Alloc32* Next;
	void* RealPtr;
	uint32_t Used;
	uint32_t Count;
	uint32_t Free;
	uint32_t Padding[7];
	Data32 Data[ALLOC32_MAX];
};

static_assert(ALLOC32_SIZE == ALLOC32_EXPECTED_SIZE, "Alloc32 size mismatch");


typedef struct Alloc64 Alloc64;

#ifndef ALLOC64_MAX
	#define ALLOC64_MAX UINT32_C(16383)
	#define ALLOC64_EXPECTED_SIZE 0x100000
#endif

#define ALLOC64_SIZE sizeof(Alloc64)
#define ALLOC64_MASK (ALLOC64_SIZE - 1)

typedef struct Data64
{
	uint32_t Data[16];
}
Data64;

struct Alloc64
{
	Alloc64* Prev;
	Alloc64* Next;
	void* RealPtr;
	uint32_t Used;
	uint32_t Count;
	uint32_t Free;
	uint32_t Padding[7];
	Data64 Data[ALLOC64_MAX];
};

static_assert(ALLOC64_SIZE == ALLOC64_EXPECTED_SIZE, "Alloc64 size mismatch");


typedef struct Alloc128 Alloc128;

#ifndef ALLOC128_MAX
	#define ALLOC128_MAX UINT32_C(8191)
	#define ALLOC128_EXPECTED_SIZE 0x100000
#endif

#define ALLOC128_SIZE sizeof(Alloc128)
#define ALLOC128_MASK (ALLOC128_SIZE - 1)

typedef struct Data128
{
	uint32_t Data[32];
}
Data128;

struct Alloc128
{
	Alloc128* Prev;
	Alloc128* Next;
	void* RealPtr;
	uint32_t Used;
	uint32_t Count;
	uint32_t Free;
	uint32_t Padding[23];
	Data128 Data[ALLOC128_MAX];
};

static_assert(ALLOC128_SIZE == ALLOC128_EXPECTED_SIZE, "Alloc128 size mismatch");


typedef struct Alloc256 Alloc256;

#ifndef ALLOC256_MAX
	#define ALLOC256_MAX UINT32_C(4095)
	#define ALLOC256_EXPECTED_SIZE 0x100000
#endif

#define ALLOC256_SIZE sizeof(Alloc256)
#define ALLOC256_MASK (ALLOC256_SIZE - 1)

typedef struct Data256
{
	uint32_t Data[64];
}
Data256;

struct Alloc256
{
	Alloc256* Prev;
	Alloc256* Next;
	void* RealPtr;
	uint32_t Used;
	uint32_t Count;
	uint32_t Free;
	uint32_t Padding[55];
	Data256 Data[ALLOC256_MAX];
};

static_assert(ALLOC256_SIZE == ALLOC256_EXPECTED_SIZE, "Alloc256 size mismatch");


typedef struct Alloc512 Alloc512;

#ifndef ALLOC512_MAX
	#define ALLOC512_MAX UINT32_C(2047)
	#define ALLOC512_EXPECTED_SIZE 0x100000
#endif

#define ALLOC512_SIZE sizeof(Alloc512)
#define ALLOC512_MASK (ALLOC512_SIZE - 1)

typedef struct Data512
{
	uint32_t Data[128];
}
Data512;

struct Alloc512
{
	Alloc512* Prev;
	Alloc512* Next;
	void* RealPtr;
	uint32_t Used;
	uint32_t Count;
	uint32_t Free;
	uint32_t Padding[119];
	Data512 Data[ALLOC512_MAX];
};

static_assert(ALLOC512_SIZE == ALLOC512_EXPECTED_SIZE, "Alloc512 size mismatch");


typedef struct Alloc1024 Alloc1024;

#ifndef ALLOC1024_MAX
	#define ALLOC1024_MAX UINT32_C(1023)
	#define ALLOC1024_EXPECTED_SIZE 0x100000
#endif

#define ALLOC1024_SIZE sizeof(Alloc1024)
#define ALLOC1024_MASK (ALLOC1024_SIZE - 1)

typedef struct Data1024
{
	uint32_t Data[256];
}
Data1024;

struct Alloc1024
{
	Alloc1024* Prev;
	Alloc1024* Next;
	void* RealPtr;
	uint32_t Used;
	uint32_t Count;
	uint32_t Free;
	uint32_t Padding[247];
	Data1024 Data[ALLOC1024_MAX];
};

static_assert(ALLOC1024_SIZE == ALLOC1024_EXPECTED_SIZE, "Alloc1024 size mismatch");


typedef struct Alloc2048 Alloc2048;

#ifndef ALLOC2048_MAX
	#define ALLOC2048_MAX UINT32_C(511)
	#define ALLOC2048_EXPECTED_SIZE 0x100000
#endif

#define ALLOC2048_SIZE sizeof(Alloc2048)
#define ALLOC2048_MASK (ALLOC2048_SIZE - 1)

typedef struct Data2048
{
	uint32_t Data[512];
}
Data2048;

struct Alloc2048
{
	Alloc2048* Prev;
	Alloc2048* Next;
	void* RealPtr;
	uint32_t Used;
	uint32_t Count;
	uint32_t Free;
	uint32_t Padding[503];
	Data2048 Data[ALLOC2048_MAX];
};

static_assert(ALLOC2048_SIZE == ALLOC2048_EXPECTED_SIZE, "Alloc2048 size mismatch");


typedef struct Alloc4096 Alloc4096;

#ifndef ALLOC4096_MAX
	#define ALLOC4096_MAX UINT32_C(255)
	#define ALLOC4096_EXPECTED_SIZE 0x100000
#endif

#define ALLOC4096_SIZE sizeof(Alloc4096)
#define ALLOC4096_MASK (ALLOC4096_SIZE - 1)

typedef struct Data4096
{
	uint32_t Data[1024];
}
Data4096;

struct Alloc4096
{
	Alloc4096* Prev;
	Alloc4096* Next;
	void* RealPtr;
	uint32_t Used;
	uint32_t Count;
	uint32_t Free;
	uint32_t Padding[1015];
	Data4096 Data[ALLOC4096_MAX];
};

static_assert(ALLOC4096_SIZE == ALLOC4096_EXPECTED_SIZE, "Alloc4096 size mismatch");


typedef struct Alloc8192 Alloc8192;

#ifndef ALLOC8192_MAX
	#define ALLOC8192_MAX UINT32_C(127)
	#define ALLOC8192_EXPECTED_SIZE 0xFF000
#endif

#define ALLOC8192_SIZE sizeof(Alloc8192)
#define ALLOC8192_MASK (ALLOC8192_SIZE - 1)

typedef struct Data8192
{
	uint32_t Data[2048];
}
Data8192;

struct Alloc8192
{
	Alloc8192* Prev;
	Alloc8192* Next;
	void* RealPtr;
	uint32_t Used;
	uint32_t Count;
	uint32_t Free;
	uint32_t Padding[1015];
	Data8192 Data[ALLOC8192_MAX];
};

static_assert(ALLOC8192_SIZE == ALLOC8192_EXPECTED_SIZE, "Alloc8192 size mismatch");


typedef struct AllocInfo
{
#ifdef ALLOC_THREADS
	pthread_mutex_t Mutex;
#endif

	uint64_t Count;
	uint64_t AllocCount;

	struct
	{
		void* Prev;
		void* Next;
	}
	*Head;
}
AllocInfo;

static AllocInfo Allocs[14] = {0};


#define ALLOC_MACRO(Size)													\
{																			\
	Alloc##Size * Alloc = (void*) Info->Head;								\
	if(!Alloc)																\
	{																		\
		void* TempAlloc = mmap(NULL, ALLOC##Size##_SIZE * 2,				\
			PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);	\
		if(TempAlloc == MAP_FAILED)											\
		{																	\
			RetPtr = NULL;													\
			break;															\
		}																	\
																			\
		Alloc = (void*)(((uintptr_t) TempAlloc +							\
			ALLOC##Size##_MASK ) & ~ ALLOC##Size##_MASK );					\
		Alloc->Prev = NULL;													\
		Alloc->Next = NULL;													\
		Alloc->RealPtr = TempAlloc;											\
		Alloc->Used = 0;													\
		Alloc->Count = 0;													\
		Alloc->Free = UINT32_MAX;											\
																			\
		++Info->AllocCount;													\
		Info->Head = (void*) Alloc;											\
	}																		\
																			\
	++Info->Count;															\
	++Alloc->Count;															\
																			\
	if(Alloc->Count == ALLOC##Size##_MAX )									\
	{																		\
		Info->Head = (void*) Alloc->Next;									\
																			\
		if(Alloc->Next)														\
		{																	\
			Alloc->Next->Prev = NULL;										\
		}																	\
	}																		\
																			\
	if(Alloc->Free != UINT32_MAX)											\
	{																		\
		Data##Size * Ptr = Alloc->Data + Alloc->Free;						\
		Alloc->Free = Ptr->Data[0];											\
																			\
		if(Zero)															\
		{																	\
			*Ptr = ( Data##Size ){0};										\
		}																	\
																			\
		RetPtr = Ptr;														\
		break;																\
	}																		\
																			\
	RetPtr = Alloc->Data + Alloc->Used++;									\
	break;																	\
}


static void*
Alloc(
	uint64_t Size,
	int Zero
	)
{
	if(Size == 0)
	{
		return NULL;
	}

	uint64_t PO2_Size;
	if(Size > 2)
	{
		PO2_Size = 1 << (32 - __builtin_clzll(Size - 1));
	}
	else
	{
		PO2_Size = Size;
	}

	uint8_t Index = __builtin_ctzll(PO2_Size);
	AllocInfo* Info = &Allocs[Index];

	void* RetPtr;

#ifdef ALLOC_THREADS
	(void) pthread_mutex_lock(&Info->Mutex);
#endif


	switch(Index)
	{

	case 0:
	{
		Alloc1Block* Block = (void*) Info->Head;
		if(!Block)
		{
			Block = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			if(Block == MAP_FAILED)
			{
				RetPtr = NULL;
				break;
			}

			Block->Prev = NULL;
			Block->Next = NULL;
			Block->Count = 0;
			Block->Free = 0;

			for(uint8_t i = 0; i < 15; i++)
			{
				Block->Allocs[i].Next = i + 1;
				Block->Allocs[i].Used = 0;
				Block->Allocs[i].Count = 0;
				Block->Allocs[i].Free = UINT8_MAX;
			}

			Block->Allocs[15].Next = UINT8_MAX;
			Block->Allocs[15].Used = 0;
			Block->Allocs[15].Count = 0;
			Block->Allocs[15].Free = UINT8_MAX;

			++Info->AllocCount;
			Info->Head = (void*) Block;
		}

		Alloc1* Alloc = &Block->Allocs[Block->Free];

		++Info->Count;
		++Block->Count;
		++Alloc->Count;

		if(Alloc->Count == 250)
		{
			if(Block->Count == 250 * 16)
			{
				Info->Head = (void*) Block->Next;

				if(Block->Next)
				{
					Block->Next->Prev = NULL;
				}
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

			RetPtr = Ptr;
			break;
		}

		RetPtr = Alloc->Data + Alloc->Used++;
		break;
	}

	case 1:
	{
		Alloc2* Alloc = (void*) Info->Head;
		if(!Alloc)
		{
			void* TempAlloc = mmap(NULL, ALLOC2_SIZE * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			if(TempAlloc == MAP_FAILED)
			{
				RetPtr = NULL;
				break;
			}

			Alloc = (void*)(((uintptr_t) TempAlloc + ALLOC2_MASK) & ~ALLOC2_MASK);
			Alloc->Prev = NULL;
			Alloc->Next = NULL;
			Alloc->RealPtr = TempAlloc;
			Alloc->Used = 0;
			Alloc->Count = 0;
			Alloc->Free = UINT16_MAX;

			++Info->AllocCount;
			Info->Head = (void*) Alloc;
		}

		++Info->Count;
		++Alloc->Count;

		if(Alloc->Count == ALLOC2_MAX)
		{
			Info->Head = (void*) Alloc->Next;

			if(Alloc->Next)
			{
				Alloc->Next->Prev = NULL;
			}
		}

		if(Alloc->Free != UINT16_MAX)
		{
			uint16_t* Ptr = Alloc->Data + Alloc->Free;
			Alloc->Free = *Ptr;

			if(Zero)
			{
				*Ptr = 0;
			}

			RetPtr = Ptr;
			break;
		}

		RetPtr = Alloc->Data + Alloc->Used++;
		break;
	}

	case 2: ALLOC_MACRO(4)
	case 3: ALLOC_MACRO(8)
	case 4: ALLOC_MACRO(16)
	case 5: ALLOC_MACRO(32)
	case 6: ALLOC_MACRO(64)
	case 7: ALLOC_MACRO(128)
	case 8: ALLOC_MACRO(256)
	case 9: ALLOC_MACRO(512)
	case 10: ALLOC_MACRO(1024)
	case 11: ALLOC_MACRO(2048)
	case 12: ALLOC_MACRO(4096)
	case 13: ALLOC_MACRO(8192)

	default:
	{
		uint64_t PageSize = (Size + 0xFFF) & ~0xFFF;
		RetPtr = mmap(NULL, PageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		break;
	}

	}


#ifdef ALLOC_THREADS
	(void) pthread_mutex_unlock(&Info->Mutex);
#endif

	return RetPtr;
}


#undef ALLOC_MACRO


void*
Malloc(
	uint64_t Size
	)
{
	return Alloc(Size, 0);
}


void*
Calloc(
	uint64_t Size
	)
{
	return Alloc(Size, 1);
}


#define ALLOC_MACRO(Size)											\
{																	\
	Alloc##Size * Alloc = ( Alloc##Size *)							\
		((uintptr_t) Ptr & ~ ALLOC##Size##_MASK );					\
																	\
	--Info->Count;													\
	--Alloc->Count;													\
																	\
	if(Alloc->Count == ALLOC##Size##_MAX - 1)						\
	{																\
		if(Info->Head)												\
		{															\
			Info->Head->Prev = Alloc;								\
		}															\
																	\
		Alloc->Prev = NULL;											\
		Alloc->Next = (void*) Info->Head;							\
		Info->Head = (void*) Alloc;									\
	}																\
	else if(Alloc->Count == 0 && Info->AllocCount >= 2 &&			\
		Info->Count <= ALLOC##Size##_MAX * (Info->AllocCount - 2))	\
	{																\
		if(Alloc->Prev)												\
		{															\
			Alloc->Prev->Next = Alloc->Next;						\
		}															\
		else														\
		{															\
			Info->Head = (void*) Alloc->Next;						\
		}															\
																	\
		if(Alloc->Next)												\
		{															\
			Alloc->Next->Prev = Alloc->Prev;						\
		}															\
																	\
		(void) munmap(Alloc->RealPtr, ALLOC##Size##_SIZE * 2);		\
		--Info->AllocCount;											\
	}																\
	else															\
	{																\
		Data##Size * DataPtr = Ptr;									\
		DataPtr->Data[0] = Alloc->Free;								\
		Alloc->Free = DataPtr - Alloc->Data;						\
	}																\
																	\
	break;															\
}


void
Free(
	void* Ptr,
	uint64_t Size
	)
{
	if(!Ptr)
	{
		return;
	}

	uint64_t PO2_Size;
	if(Size > 2)
	{
		PO2_Size = 1 << (32 - __builtin_clzll(Size - 1));
	}
	else
	{
		PO2_Size = Size;
	}

	uint8_t Index = __builtin_ctzll(PO2_Size);
	AllocInfo* Info = &Allocs[Index];

#ifdef ALLOC_THREADS
	(void) pthread_mutex_lock(&Info->Mutex);
#endif


	switch(Index)
	{

	case 0:
	{
		Alloc1Block* Block = (Alloc1Block*)((uintptr_t) Ptr & ~0xFFF);
		Alloc1* Alloc = &Block->Allocs[((uintptr_t) Ptr - (uintptr_t) Block - 0x20) / 254];

		--Info->Count;
		--Block->Count;
		--Alloc->Count;

		if(Alloc->Count == 250 - 1)
		{
			Alloc->Next = Block->Free;
			Block->Free = Alloc - Block->Allocs;
		}

		if(Block->Count == 250 * 16 - 1)
		{
			if(Info->Head)
			{
				Info->Head->Prev = Block;
			}

			Block->Prev = NULL;
			Block->Next = (void*) Info->Head;
			Info->Head = (void*) Block;
		}
		else if(Block->Count == 0 && Info->AllocCount >= 2 && Info->Count <= 250 * 16 * (Info->AllocCount - 2))
		{
			if(Block->Prev)
			{
				Block->Prev->Next = Block->Next;
			}
			else
			{
				Info->Head = (void*) Block->Next;
			}

			if(Block->Next)
			{
				Block->Next->Prev = Block->Prev;
			}

			(void) munmap(Block, 4096);
			--Info->AllocCount;
		}
		else
		{
			*((uint8_t*) Ptr) = Alloc->Free;
			Alloc->Free = (uint8_t*) Ptr - Alloc->Data;
		}

		break;
	}

	case 1:
	{
		Alloc2* Alloc = (Alloc2*)((uintptr_t) Ptr & ~ALLOC2_MASK);

		--Info->Count;
		--Alloc->Count;

		if(Alloc->Count == ALLOC2_MAX - 1)
		{
			if(Info->Head)
			{
				Info->Head->Prev = Alloc;
			}

			Alloc->Prev = NULL;
			Alloc->Next = (void*) Info->Head;
			Info->Head = (void*) Alloc;
		}
		else if(Alloc->Count == 0 && Info->AllocCount >= 2 && Info->Count <= ALLOC2_MAX * (Info->AllocCount - 2))
		{
			if(Alloc->Prev)
			{
				Alloc->Prev->Next = Alloc->Next;
			}
			else
			{
				Info->Head = (void*) Alloc->Next;
			}

			if(Alloc->Next)
			{
				Alloc->Next->Prev = Alloc->Prev;
			}

			(void) munmap(Alloc->RealPtr, ALLOC2_SIZE * 2);
			--Info->AllocCount;
		}
		else
		{
			*((uint16_t*) Ptr) = Alloc->Free;
			Alloc->Free = (uint16_t*) Ptr - Alloc->Data;
		}

		break;
	}

	case 2: ALLOC_MACRO(4)
	case 3: ALLOC_MACRO(8)
	case 4: ALLOC_MACRO(16)
	case 5: ALLOC_MACRO(32)
	case 6: ALLOC_MACRO(64)
	case 7: ALLOC_MACRO(128)
	case 8: ALLOC_MACRO(256)
	case 9: ALLOC_MACRO(512)
	case 10: ALLOC_MACRO(1024)
	case 11: ALLOC_MACRO(2048)
	case 12: ALLOC_MACRO(4096)
	case 13: ALLOC_MACRO(8192)

	default:
	{
		uint64_t PageSize = (Size + 0xFFF) & ~0xFFF;
		(void) munmap(Ptr, PageSize);
		break;
	}

	}


#ifdef ALLOC_THREADS
	(void) pthread_mutex_unlock(&Info->Mutex);
#endif
}


#undef ALLOC_MACRO


void*
Remalloc(
	void* Ptr,
	uint64_t OldSize,
	uint64_t NewSize
	)
{
	if(!Ptr)
	{
		return Malloc(NewSize);
	}

	uint64_t PO2_OldSize;
	if(OldSize > 2)
	{
		PO2_OldSize = 1 << (32 - __builtin_clzll(OldSize - 1));
	}
	else
	{
		PO2_OldSize = OldSize;
	}

	uint8_t OldIndex = __builtin_ctzll(PO2_OldSize);

	uint64_t PO2_NewSize;
	if(NewSize > 2)
	{
		PO2_NewSize = 1 << (32 - __builtin_clzll(NewSize - 1));
	}
	else
	{
		PO2_NewSize = NewSize;
	}

	uint8_t NewIndex = __builtin_ctzll(PO2_NewSize);

	if(PO2_OldSize == PO2_NewSize && OldIndex < 14 && NewIndex < 14)
	{
		return Ptr;
	}

	void* NewPtr = Malloc(NewSize);
	if(!NewPtr)
	{
		return NULL;
	}

	(void) memcpy(NewPtr, Ptr, OldSize);
	Free(Ptr, OldSize);

	return NewPtr;
}


void*
Recalloc(
	void* Ptr,
	uint64_t OldSize,
	uint64_t NewSize
	)
{
	if(!Ptr)
	{
		return Calloc(NewSize);
	}

	uint64_t PO2_OldSize;
	if(OldSize > 2)
	{
		PO2_OldSize = 1 << (32 - __builtin_clzll(OldSize - 1));
	}
	else
	{
		PO2_OldSize = OldSize;
	}

	uint8_t OldIndex = __builtin_ctzll(PO2_OldSize);

	uint64_t PO2_NewSize;
	if(NewSize > 2)
	{
		PO2_NewSize = 1 << (32 - __builtin_clzll(NewSize - 1));
	}
	else
	{
		PO2_NewSize = NewSize;
	}

	uint8_t NewIndex = __builtin_ctzll(PO2_NewSize);

	if(PO2_OldSize == PO2_NewSize && OldIndex < 14 && NewIndex < 14)
	{
		(void) memset(Ptr + OldSize, 0, NewSize - OldSize);
		return Ptr;
	}

	void* NewPtr = Malloc(NewSize);
	if(!NewPtr)
	{
		return NULL;
	}

	(void) memcpy(NewPtr, Ptr, OldSize);
	Free(Ptr, OldSize);

	(void) memset(NewPtr + OldSize, 0, NewSize - OldSize);
	return NewPtr;
}

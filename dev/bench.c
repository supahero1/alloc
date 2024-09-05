#include "dev.c"

#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <threads.h>

#define OPERATIONS 0x100000
#define THREADS 0x10
#define POINTERS 0x80


struct
{
	mtx_t Mutex;
	void* Ptr;
	size_t Size;
}
Ptrs[POINTERS];


int
dev_bench(
	void* Arg
	)
{
	(void) Arg;

	for(int i = 0; i < OPERATIONS; i++)
	{
		int r = rand();
		int Index = r % POINTERS;
		r >>= 12;
		int Size = 1 + (r & 0xFFFF);
		r >>= 16;
		int Bool = r & 1;

		mtx_lock(&Ptrs[Index].Mutex);

		if(Ptrs[Index].Ptr)
		{
			if(Bool)
			{
				Ptrs[Index].Ptr = dev_realloc(Ptrs[Index].Ptr, Ptrs[Index].Size, Size, 0);
				Ptrs[Index].Size = Size;
			}
			else
			{
				dev_free(Ptrs[Index].Ptr, Ptrs[Index].Size);
				Ptrs[Index].Ptr = NULL;
			}
		}
		else
		{
			Ptrs[Index].Size = Size;
			Ptrs[Index].Ptr = dev_alloc(Ptrs[Index].Size, 0);
		}

		mtx_unlock(&Ptrs[Index].Mutex);
	}

	return 0;
}


int
main(
	void
	)
{
	srand(time(NULL));

	for(int i = 0; i < POINTERS; i++)
	{
		mtx_init(&Ptrs[i].Mutex, mtx_plain);
		Ptrs[i].Ptr = NULL;
		Ptrs[i].Size = 0;
	}

	puts("start");

	thrd_t Threads[THREADS];

	for(int i = 0; i < THREADS; i++)
	{
		thrd_create(&Threads[i], dev_bench, NULL);
	}

	for(int i = 0; i < THREADS; i++)
	{
		thrd_join(Threads[i], NULL);
	}

	puts("end");

	for(int i = 0; i < POINTERS; i++)
	{
		mtx_destroy(&Ptrs[i].Mutex);
	}

	return 0;
}

#include "dev.c"

#include <time.h>
#include <stdio.h>
#include <pthread.h>

#define OPERATIONS 0x10000
#define THREADS 0x10
#define POINTERS 0x1000


static uint32_t r_seed;

static void
fast_srand(
	const uint32_t seed
	)
{
	r_seed = seed;
}

static uint32_t
fast_rand(
	void
	)
{
	/* Yes this is multithreaded, yes corruption will occur, no I do not care */
	r_seed = (1103515245 * r_seed + 12345) & 0x7FFFFFFF;
	return r_seed;
}


struct
{
	pthread_mutex_t Mutex;
	void* Ptr;
	size_t Size;
}
Ptrs[POINTERS];


void*
dev_bench(
	void* Arg
	)
{
	(void) Arg;

	for(int i = 0; i < OPERATIONS; i++)
	{
		int r = fast_rand();
		int Index = r % POINTERS;
		r >>= 12;
		int Size = 1 + (r & 0xFFFF);
		r >>= 16;
		int Bool = r & 1;

		pthread_mutex_lock(&Ptrs[Index].Mutex);

		if(Ptrs[Index].Ptr)
		{
			if(Bool)
			{
				Ptrs[Index].Ptr = dev_realloc(Ptrs[Index].Ptr, Ptrs[Index].Size, Size, 0);
				*(uint8_t*) Ptrs[Index].Ptr = 0;
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
			*(uint8_t*) Ptrs[Index].Ptr = 0;
		}

		pthread_mutex_unlock(&Ptrs[Index].Mutex);
	}

	return NULL;
}


int
main(
	void
	)
{
	fast_srand(time(NULL));

	for(int i = 0; i < POINTERS; i++)
	{
		pthread_mutex_init(&Ptrs[i].Mutex, NULL);
		Ptrs[i].Ptr = NULL;
		Ptrs[i].Size = 0;
	}

	puts("start");

	pthread_t Threads[THREADS];

	for(int i = 0; i < THREADS; i++)
	{
		pthread_create(&Threads[i], NULL, dev_bench, NULL);
	}

	for(int i = 0; i < THREADS; i++)
	{
		pthread_join(Threads[i], NULL);
	}

	puts("end");

	for(int i = 0; i < POINTERS; i++)
	{
		pthread_mutex_destroy(&Ptrs[i].Mutex);
	}

	return 0;
}

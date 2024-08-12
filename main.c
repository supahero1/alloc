#include "alloc.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>


static uint64_t
GetTime(
	void
	)
{
	struct timespec Time;
	clock_gettime(CLOCK_MONOTONIC, &Time);
	return Time.tv_sec * 1000000000 + Time.tv_nsec;
}


static void
PrintTime(
	uint64_t Start,
	uint64_t End
	)
{
	uint64_t Diff = End - Start;
	printf("Time: %.02fms\n", (double) Diff / 1000000);
}


static void
Benchmark(
	uint64_t Size,
	uint64_t Memory
	)
{
	uint64_t Start, End;
	uint64_t i;
	uint64_t Num = Memory / Size;
	void** Ptrs = malloc(sizeof(void*) * Num);
	uint64_t* Rand = malloc(sizeof(uint64_t) * Num);

	for(i = 0; i < Num; i++)
	{
		Rand[i] = i;
	}

	for(i = 0; i < Num; i++)
	{
		uint64_t Temp = Rand[i];
		uint64_t Index = rand() % Num;
		Rand[i] = Rand[Index];
		Rand[Index] = Temp;
	}

	printf("\n> Benchmark(%lu, %lu)\n", Size, Memory);


	{
		puts("Malloc");
		Start = GetTime();

		for(i = 0; i < Num; i++)
		{
			Ptrs[i] = Malloc(Size);
		}

		End = GetTime();
		PrintTime(Start, End);


		puts("Free(in order)");
		Start = GetTime();

		for(i = 0; i < Num; i++)
		{
			Free(Ptrs[i], Size);
		}

		End = GetTime();
		PrintTime(Start, End);


		puts("Malloc");
		Start = GetTime();

		for(i = 0; i < Num; i++)
		{
			Ptrs[i] = Malloc(Size);
		}

		End = GetTime();
		PrintTime(Start, End);


		puts("Free(random)");
		Start = GetTime();

		for(i = 0; i < Num; i++)
		{
			Free(Ptrs[Rand[i]], Size);
		}

		End = GetTime();
		PrintTime(Start, End);


		puts("Malloc");
		Start = GetTime();

		for(i = 0; i < Num; i++)
		{
			Ptrs[i] = Malloc(Size);
		}

		End = GetTime();
		PrintTime(Start, End);


		puts("Free(reverse order)");
		Start = GetTime();

		for(i = Num; i > 0; i--)
		{
			Free(Ptrs[i - 1], Size);
		}

		End = GetTime();
		PrintTime(Start, End);


		puts("Malloc");
		Start = GetTime();

		for(i = 0; i < Num; i++)
		{
			Ptrs[i] = Malloc(Size);
		}

		End = GetTime();
		PrintTime(Start, End);


		puts("Linear walk");
		Start = GetTime();

		for(i = 0; i < Num; i++)
		{
			*(uint8_t*)Ptrs[i] = i;
		}

		End = GetTime();
		PrintTime(Start, End);


		puts("Free");
		Start = GetTime();

		for(i = 0; i < Num; i++)
		{
			Free(Ptrs[i], Size);
		}

		End = GetTime();
		PrintTime(Start, End);
	}

	puts("");

	{
		puts("malloc");
		Start = GetTime();

		for(i = 0; i < Num; i++)
		{
			Ptrs[i] = malloc(Size);
		}

		End = GetTime();
		PrintTime(Start, End);


		puts("free(in order)");
		Start = GetTime();

		for(i = 0; i < Num; i++)
		{
			free(Ptrs[i]);
		}

		End = GetTime();
		PrintTime(Start, End);


		puts("malloc");
		Start = GetTime();

		for(i = 0; i < Num; i++)
		{
			Ptrs[i] = malloc(Size);
		}

		End = GetTime();
		PrintTime(Start, End);


		puts("free(random)");
		Start = GetTime();

		for(i = 0; i < Num; i++)
		{
			free(Ptrs[Rand[i]]);
		}

		End = GetTime();
		PrintTime(Start, End);


		puts("malloc");
		Start = GetTime();

		for(i = 0; i < Num; i++)
		{
			Ptrs[i] = malloc(Size);
		}

		End = GetTime();
		PrintTime(Start, End);


		puts("free(reverse order)");
		Start = GetTime();

		for(i = Num; i > 0; i--)
		{
			free(Ptrs[i - 1]);
		}

		End = GetTime();
		PrintTime(Start, End);


		puts("malloc");
		Start = GetTime();

		for(i = 0; i < Num; i++)
		{
			Ptrs[i] = malloc(Size);
		}

		End = GetTime();
		PrintTime(Start, End);


		puts("linear walk");
		Start = GetTime();

		for(i = 0; i < Num; i++)
		{
			*(uint8_t*)Ptrs[i] = i;
		}

		End = GetTime();
		PrintTime(Start, End);


		puts("free");
		Start = GetTime();

		for(i = 0; i < Num; i++)
		{
			free(Ptrs[i]);
		}

		End = GetTime();
		PrintTime(Start, End);
	}


	free(Rand);
	free(Ptrs);
}


int
main(
	void
	)
{
	srand(time(NULL));

	Benchmark(   1, 1UL << 20);
	Benchmark(   2, 1UL << 21);
	Benchmark(   4, 1UL << 22);
	Benchmark(   8, 1UL << 23);
	Benchmark(  16, 1UL << 23);
	Benchmark(  32, 1UL << 24);
	Benchmark(  64, 1UL << 25);
	Benchmark( 128, 1UL << 26);
	Benchmark( 256, 1UL << 26);
	Benchmark( 512, 1UL << 27);
	Benchmark(1024, 1UL << 28);
	Benchmark(2048, 1UL << 29);
	Benchmark(4096, 1UL << 29);
	Benchmark(8192, 1UL << 30);

	return 0;
}

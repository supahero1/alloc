#include "dev.c"


#define MAX 0x100


void
test(
	size_t Size,
	uint8_t* Shuffle
	)
{
	uint8_t* Ptrs[256];

	for(size_t i = 0; i < 256; ++i)
	{
		Ptrs[i] = dev_alloc(Size, 1);
		AssertNEQ(Ptrs[i], NULL);

		(void) memset(Ptrs[i], Shuffle[i], Size);
	}

	for(size_t i = 0; i < 256; ++i)
	{
		for(size_t j = 0; j < 256; ++j)
		{
			if(i == j) continue;
			AssertNEQ(Ptrs[i], Ptrs[j]);
			AssertEQ((
				Ptrs[i] + Size <= Ptrs[j] || Ptrs[i] >= Ptrs[j] + Size
				), 1);
		}

		void* Tmp = dev_alloc(Size, 0);
		AssertNEQ(Tmp, NULL);

		(void) memset(Tmp, Shuffle[i], Size);
		AssertEQ(memcmp(Ptrs[i], Tmp, Size), 0);

		dev_free(Tmp, Size);
	}

	{
		void* Ptr = dev_alloc(Size, 1);
		AssertNEQ(Ptr, NULL);

		void* Zero = dev_alloc(Size, 0);
		AssertNEQ(Zero, NULL);

		(void) memset(Zero, 0, Size);
		AssertEQ(memcmp(Ptr, Zero, Size), 0);

		Ptrs[0] = dev_realloc(Ptrs[0], Size, Size << 1, 1);
		AssertNEQ(Ptrs[0], NULL);

		void* Tmp = dev_alloc(Size, 0);
		AssertNEQ(Tmp, NULL);

		(void) memset(Tmp, Shuffle[0], Size);

		AssertEQ(memcmp(Ptrs[0], Tmp, Size), 0);
		AssertEQ(memcmp(Ptrs[0] + Size, Ptr, Size), 0);

		dev_free(Ptr, Size);

		Ptrs[0] = dev_realloc(Ptrs[0], Size << 1, Size, 0);
		AssertNEQ(Ptrs[0], NULL);

		AssertEQ(memcmp(Ptrs[0], Tmp, Size), 0);

		dev_free(Tmp, Size);
	}

	for(size_t i = 0; i < 256; ++i)
	{
		dev_free(Ptrs[Shuffle[i]], Size);
	}
}


#include <time.h>
#include <unistd.h>


int
main(
	void
	)
{
	srand(time(NULL));

	uint8_t* Shuffle = dev_alloc(256, 0);
	AssertNEQ(Shuffle, NULL);

	for(size_t i = 0; i < 256; ++i)
	{
		Shuffle[i] = i;
	}

	for(size_t i = 0; i < 256; ++i)
	{
		size_t j = rand() % 256;

		uint8_t Temp = Shuffle[i];
		Shuffle[i] = Shuffle[j];
		Shuffle[j] = Temp;
	}

	for(size_t i = 1; i <= MAX; ++i)
	{
		test(i, Shuffle);
	}

	puts("pass");

	return 0;
}

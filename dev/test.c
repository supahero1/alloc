#include "dev.c"


#define MAX 0x100


void
test(
	size_t size,
	uint8_t* shuffle
	)
{
	uint8_t* ptrs[256];

	for(size_t i = 0; i < 256; ++i)
	{
		ptrs[i] = dev_alloc(size, 1);
		assert_neq(ptrs[i], NULL);

		(void) memset(ptrs[i], shuffle[i], size);
	}

	for(size_t i = 0; i < 256; ++i)
	{
		for(size_t j = 0; j < 256; ++j)
		{
			if(i == j) continue;
			assert_neq(ptrs[i], ptrs[j]);
			assert_eq((
				ptrs[i] + size <= ptrs[j] || ptrs[i] >= ptrs[j] + size
				), 1);
		}

		void* tmp = dev_alloc(size, 0);
		assert_neq(tmp, NULL);

		(void) memset(tmp, shuffle[i], size);
		assert_eq(memcmp(ptrs[i], tmp, size), 0);

		dev_free(tmp, size);
	}

	{
		void* ptr = dev_alloc(size, 1);
		assert_neq(ptr, NULL);

		void* zero = dev_alloc(size, 0);
		assert_neq(zero, NULL);

		(void) memset(zero, 0, size);
		assert_eq(memcmp(ptr, zero, size), 0);

		ptrs[0] = dev_realloc(ptrs[0], size, size << 1, 1);
		assert_neq(ptrs[0], NULL);

		void* tmp = dev_alloc(size, 0);
		assert_neq(tmp, NULL);

		(void) memset(tmp, shuffle[0], size);

		assert_eq(memcmp(ptrs[0], tmp, size), 0);
		assert_eq(memcmp(ptrs[0] + size, ptr, size), 0);

		dev_free(ptr, size);

		ptrs[0] = dev_realloc(ptrs[0], size << 1, size, 0);
		assert_neq(ptrs[0], NULL);

		assert_eq(memcmp(ptrs[0], tmp, size), 0);

		dev_free(tmp, size);
	}

	for(size_t i = 0; i < 256; ++i)
	{
		dev_free(ptrs[shuffle[i]], size);
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

	uint8_t* shuffle = dev_alloc(256, 0);
	assert_neq(shuffle, NULL);

	for(size_t i = 0; i < 256; ++i)
	{
		shuffle[i] = i;
	}

	for(size_t i = 0; i < 256; ++i)
	{
		size_t j = rand() % 256;

		uint8_t temp = shuffle[i];
		shuffle[i] = shuffle[j];
		shuffle[j] = temp;
	}

	for(size_t i = 1; i <= MAX; ++i)
	{
		test(i, shuffle);
	}

	puts("pass");

	return 0;
}

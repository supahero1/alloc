This is a proof-of-concept repository aiming to fulfill the idea of an allocator that needs very little metadata but a lot of thought behind it instead to make it all around very efficient.

The code is not finished and assumes some stuff. The basic functions are done and work, however there's a lot of room for improvement. For instance, the code runs only under Linux (but can under other OS). It assumes the page size is `4096`.

The allocator guarantees alignment of at least the next power of 2 of the size you are allocating. So, for `1` you get `1`, for `2` you get `2`, for `3` and `4` you get `4`, and so on, until `4096` (the page size), at which point it stops (anything above is aligned only to one page).

Memory ratios are at most as follows:

* `1.024` bytes per `1` sized allocation,
* `1.00023` bytes per `2` sized allocation,
* `1.000034` bytes per `3` to `4` sized allocation,
* `1.000038` bytes per `5` to `8` sized allocation,
* `1.0000458` bytes per `9` to `16` sized allocation,
* `1.00006` bytes per `17` to `64` sized allocation,
* `1.0001` bytes per `65` to `128` sized allocation,
* `1.00024` bytes per `129` to `256` sized allocation,
* `1.0005` bytes per `257` to `512` sized allocation,
* `1.001` bytes per `513` to `1024` sized allocation,
* `1.002` bytes per `1025` to `2048` sized allocation,
* `1.004` bytes per `2049` to `4096` sized allocation,
* `1.008` bytes per `4097` to `8192` sized allocation,
* `1` bytes for any larger allocations (no additional data attached by the allocator).

As memory usage grows, actual ratios approach the perfect values above (except the last one for big allocations, it's constant).

Allocations do not contain any metadata themselves, and thus can be stored perfectly linearly in memory. Only a whole block that contains a certain number of allocations contains some metadata.

Because of the strict alignment guarantees, implementing `posix_memalign()` comes down to simply allocating `max(Size, Align)` bytes.

# Benchmarks

You can just run `make` and it will benchmark the allocator against whatever you are using (probably ptmalloc or dlmalloc).

The benchmark doesn't reflect the ratio at high allocation sizes well, sadly. However you will most certainly be able to instantly tell at low sizes (from 1 to even like 512) how insane this allocator is.

# API

This allocator requires you to pass size of the allocation (whatever you gave to `Malloc()` or `Calloc()`) to `Free()`. That's like, the only downside versus normal API.

Basically, everything is like normal. You have `Malloc(uint64_t Size)`, paired with `Free(void* Ptr, uint64_t Size)`. You can also choose to use `Calloc(uint64_t Size)`, which often times can be more efficient than using `Malloc()` + `memset()`.

There's also `Remalloc(void* Ptr, uint64_t OldSize, uint64_t NewSize)` and `Recalloc(void* Ptr, uint64_t OldSize, uint64_t NewSize)`. The latter is mostly just a suggar function, a helper if you will. You can always use `Remalloc()` and `memset()`.

The functions for it are missing, but you could also be able to access internal allocation counters and allocator counters.

# Personal yap

`sbrk()` is not used at all - only `mmap()` is. This is crucial for the allocator to be what it is really. Exploiting virtual memory is what makes this allocator so good.

For each allocation size, there may be many allocators. Each allocator's capacity is predefined (compile time constant). You can change how many bytes are allocated for allocations of size above `1`. Size `1` is constant and cannot be changed because of integer overflow. You could in theory be able to decrease it, but not only is there no merit to it (one size `1` allocator is exactly one page in size, decreasing won't actually save you any memory since a page is the smallest unit), it would also massively complicate the code, so you simply can't do it. For size `2` you can only decrease the size. Here's how you can do it:

```c
#define ALLOC2_MAX UINT32_C(32753)
#define ALLOC2_EXPECTED_SIZE 0x10000
#include "alloc.h"
```

You can't increase it because of integer overflow. The default (`65521`) is the maximum before integer overflow and achieves the highest possible usable memory ratio. You can increase or decrease for any other sizes, same method as shown above, just swap the size integer in the macros (after `ALLOC`).

Note that `ALLOC_EXPECTED_SIZE` is for security. You should input the whole block size here (including data and metadata). You can see how big the block's metadata is in `alloc.c`. If the block turns out smaller or larger than that, a static assert will fire. It's extremely important for the size to be right, because the allocator's code depends on it being a power of 2. And well, it doesn't check if it's a power of 2, it just checks if you didn't enter some bogus `ALLOC_MAX` value, so you must take care of that if you modify it. The default is for all allocators above size `2` to allocate `1MiB` of virtual memory (no pre population, no physical backing until requested).

If you are seriously thinking about using this in production, which is good by the way, because I think this allocator is really strong, then you should know a bit more about it. Namely, the secret to it working well long-term (which the benchmark does not really verify) is for allocations to be more or less evenly distributed and freed in a timely manner. This of course is a dream, nobody is going to spend time making careful, strategic allocations and deallocations. So instead, the secret is to clump similar allocations together in their own allocators. Now, this is very easily achievable. What I mean is adding another dimension to `Allocs` in `alloc.c` array. For now it only has one dimension and that is the allocation size. In serious production, there should be another dimension that is indexed by something that you can call... lifetime, or "class" I guess? You can call it whatever you want, but it's best if you clump similar allocations and deallocations together as I said previously, so this is crucial. After adding a dimension there, you simply add another argument to `Malloc()` and `Free()` besides size that also specifies the family or lifetime or whatever, and it's done (the rest of the code needs no adjustments, it will just all start working). That another argument will most of the time be a compile time constant, and you can make it be an enum so that you can easily tell apart what it is instead of just some number.

The above requires a bit of work. Passing allocation size to realloc and free by itself could be considered a lot of work, since it's not standard, but it really does speed up the allocator and allow less useless metadata (size is often a compile-time constant too). But this is a highly performant allocator, and I'm not about to make any tradeoffs. This is what high performance needs.

Note that high performance doesn't just mean fast malloc and free. If you've ran the benchmark, you know the INSANE advantage this allocator has for small allocations just because every allocation has no metadata, so if you make a bunch of them and walk through them (like a linked list for instance), you get a lot more cache friendly, leading to faster code execution. And it's also nice to have high usable memory ratio.

If you do the additional dimension deal above, you aren't only getting better memory layout over time, but also better multithreading. That's because every allocator has its own lock. They don't mingle with each other at all. The only thing they mingle with is the head structure that contains the pointer to the first allocator that has some free spots in it, and the structure's performance counters, but that structure has a separate instance for each allocation size, and would also have for the additional dimension specified above. That structure contains the mutex lock, so you would only get locked if you tried accessing same size and same family at the same time, in all other cases you pass through flawlessly.

The allocator is fully Windows compliant, just have to swap a couple functions (VirtualAlloc and SRWLOCK need to be used where appropriate).

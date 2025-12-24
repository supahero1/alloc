# C

## Usage

The allocator is written in C, so to build it, execute `make` in the root directory. That will create `bin/liballoc.so` as a debug build, with assertions all over the code. You can also run `RELEASE=1 make` to create a release build with no debug symbols. The allocator is not greately affected by a lack of optimizations (it's extremely simple), so it is recommended to go for a debug build, and only switch to release if necessary.

Note that this is not your usual all-fits-one solution allocator - for once, you have to pass allocation size to deallocation functions.

For headers, `include/alloc_std.h` is the main one, containing all the functions that the library exposes. However, it is tedious to use the barebone library by itself, so there is also `include/alloc_ext.h` that contains a large set of inline functions that can help reduce the amount of code you write. That header does not define anything new and only uses functions available already in `include/alloc_std.h`.

Building the code by default will also test it. The test does not take long and also tests your default allocator (ptmalloc on linux distributions using glibc), jemalloc (if found), and mimalloc (if found), to make sure everything is consistent. There is also an option to benchmark it using `make bench`. The benchmark is by default multithreaded. It used to be singlethreaded in a previous release, but nonetheless in both versions this allocator wins over dlmalloc.

As a shortcut (mostly for testing), you can also create a 32bit build using the `M32=1` env var (this applies to all of building, testing, and benchmarking). Note that if you've already built the library, you would first need to clear the old files with `make clean` for it to be rebuilt.

## More info

This is a general purpose passively concurrent allocator. It's pretty fast.

Everything is split between states and handles. States are just wrappers for handles, with a function (`alloc_index_fn_t`) that maps a given allocation size to a given index within the handle array that you create yourself by specifying their number and their properties (block size, object size, alignment). There's always a default global state that is used for all calls you make that you don't specify a state to directly. Functions that accept a state are always postfixed with `s` like `alloc_get_handle_s()`.

Handles are the actual allocators that do the memory management. They are extremely lightweight when not in use and allocate blocks of size `ALLOC_DEFAULT_BLOCK_SIZE` or smaller whenever necessary. They use a simple free list to keep track of freed objects, and a mutex to ensure thread safety. Unlocked methods are postfixed with `u` and functions accepting a handle explicitly are postfixed with `h`, like `alloc_alloc_uh()`.

The default flow goes something like this:

```c
alloc_alloc_h(
	alloc_get_handle_s(
		alloc_get_global_state(),
		size
		),
	size
	);
```

This is hidden behind the macros in `alloc_ext.h`, so you can just do:

```c
alloc_malloc(size);
// or
alloc_calloc(size);
```

If you run into multithread-related bottlenecks, which is frankly staggering to happen unless all threads happen to use the same allocation size for some reason, you can create your own states and handles to decrease lock contention. (See the Optimizations section below for more information). To prove how staggering that is, I have experimented with making a state thread-local, and the performance literally dropped 5 times, because tls costs more than the lock contention that was being avoided (which was close to none to begin with).

Allocations don't have any metadata except the slab-level header metadata and other metadata like the handle itself. So assuming a full slab (allocations increase to infinity), you will get ratios close to the following (they differ a little based on architecture and block size, the following is 64bit 1MiB block size):

| Allocation Size | Block size | Overhead per byte | Possible allocations |
|-----------------|------------|-------------------|----------------------|
| 1               | 4096       | 2.34%             | 4000                 |
| 1               | 65536      | 1.94%             | 64262                |
| 2               | 131072     | 0.023%            | 65521                |
| 3               | 131072     | 0.024%            | 43680                |
| 4               | 1MiB       | 0.003%            | 262135               |
| 8	              | 1MiB       | 0.004%            | 131067               |
| 16              | 1MiB       | 0.005%            | 65533                |
| 32              | 1MiB       | 0.006%            | 32766                |
| 64              | 1MiB       | 0.006%            | 16383                |
| 128             | 1MiB       | 0.012%            | 8191                 |
| 256             | 1MiB       | 0.024%            | 4095                 |
| 512             | 1MiB       | 0.049%            | 2047                 |
| 1024            | 1MiB       | 0.098%            | 1023                 |
| 2048            | 1MiB       | 0.195%            | 511                  |
| 4096            | 1MiB       | 0.391%            | 255                  |
| 8192            | 1MiB       | 0.392%            | 127                  |
| 16384           | 1MiB       | 0.395%            | 63                   |
| 32768           | 1MiB       | 0.402%            | 31                   |
| 65536           | 1MiB       | 0.415%            | 15                   |
| 131072          | 1MiB       | 0.444%            | 7                    |
| 262144          | 1MiB       | 0.518%            | 3                    |
| 524288          | 1MiB       | 0.775%            | 1                    |

Assumed alignment 1 for 3 byte allocation size. As you can see, the overhead is extremely low, even for the smallest allocation sizes, whereas other general purpose allocators would give you 32 bytes or more per every single allocation.

Note that the percentages will generally decrease for sizes greater or equal to 4 the higher the block size is, but that also increases fragmentation hazards. The block sizes for below 4 byte allocations are capped due to numerical limits.

## Optimizations

1. Increasing `ALLOC_DEFAULT_BLOCK_SIZE` at `alloc.c:18` or defining it during your build process might make the allocator marginally faster overall and significantly faster for allocation sizes 2 times smaller than the value and lower, but will increase potential fragmentation hazards. Do this if you plan on using a lot (gigabytes) of memory of varying sizes (not just one or two continuous super large allocations).

2. Creating additional states and handles will decrease lock contention and thus increase multithread efficiency, but requires you to manually pass them to relevant functions, which otherwise is not required with `alloc_ext.h` (the implicit global state is used). Note that the global state already consists of a number of different handles, so applications that do not have much choice (like Rust's `GlobalAlloc`) still receive some degree of multithread efficiency. It's just that you can increase it further manually.

3. You can create your own handles and states with own configuration of block size, object size, and alignment. The global state contains handles that allocate powers of 2 only, so if your application allocates a lot of objects of size `192`, it might be worth to create your own handle/state with that as the `AllocSize` to improve cache friendliness and use less memory.

4. Fragmentation can be decreased by categorizing allocations into, for instance, frequency, lifetime, or (ideally) both. Objects of similar traits should be clumped together into the same state/handle. To give you a further idea of what this means, see [this](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkSystemAllocationScope.html) Vulkan manual page about allocation scope (i.e. lifetime). Since this is purely optional, it is not natively supported by the library. You can however implement simple inline functions that accept an `enum AllocType` and use that type to then index an array (`kAllocType` sized) of `AllocState` objects. After choosing one (i.e. appropriate for the allocation type), tail call a library function like usual, just that instead of using the global state, you will be using an explicit state (`S` function suffix).

5. The library is already largely suited to do that, but you can explicitly cache handles so that the library does not have to retrieve them from states upon every function call, given through benchmarking you notice that this is the bottleneck (very unlikely unless using custom state with a custom, possibly expensive, `alloc_index_fn`).

## Standard API

To satisfy memory alignment guarantees for the global state, but at the same time keep an allocation size alongside the allocation to get rid of size fields in `alloc_realloc` and `alloc_free`, all memory allocations would have to be **AT LEAST** doubled in size. That is obviously not feasible and chances are that the default heap allocator, whatever it is, will perform better long-term.

Sadly, most software nowadays is suited towards the standard OG API that is not sized. Not much I can do about that. I wish I could use this allocator in Vulkan, or SDL, but oh well.

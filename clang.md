## C

The allocator is written in C, so to build it, execute `make` in the root directory. That will create `bin/liballoc.so` as a debug build, with assertions all over the code. You can also run `RELEASE=1 make` to create a release build with no debug symbols. The allocator is not greately affected by a lack of optimizations (it's extremely simple), so it is recommended to go for a debug build, and only switch to release if necessary.

There are comments all over `src/alloc.c` that you can inspect for some initial knowledge of the allocator. Note that this is not your usual all-fits-one solution allocator - for once, you have to pass allocation size to deallocation functions.

For headers, `include/alloc_std.h` is the main one, containing all the functions that the library exposes. However, it is tedious to use the barebone library by itself, so there is also `include/alloc_ext.h` that contains a large set of inline functions that can help reduce the amount of code you write. That header does not define anything new and only uses functions available already in `include/alloc_std.h`.

Building the code by default will also test it. The test does not take long and also tests your default allocator (ptmalloc on linux distributions using glibc), jemalloc (if found), and mimalloc (if found), to make sure everything is consistent. There is also an option to benchmark it using `make bench`. The benchmark is by default multithreaded. It used to be singlethreaded in a previous release, but nonetheless in both versions this allocator wins over dlmalloc.

As a shortcut (mostly for testing), you can also create a 32bit build using the `M32=1` env var (this applies to all of building, testing, and benchmarking). Note that if you've already built the library, you would first need to clear the old files with `make clean` for it to be rebuilt.

## Optimizations

1. Increasing `ALLOC_DEFAULT_BLOCK_SIZE` at `alloc.c:528` might make the allocator marginally faster overall and significantly faster for allocation sizes 4 times lower than the value, but will increase potential fragmentation hazards. Do this if you plan on using a lot (gigabytes) of memory of varying sizes (not just one or two continuous super large allocations).

2. Creating additional states and handles will decrease lock contention and thus increase multithread efficiency, but requires you to manually pass them to relevant functions, which otherwise is not required with `alloc_ext.h` (the implicit global state is used). Note that the global state already consists of a number of different handles, so applications that do not have much choice (like Rust's `GlobalAlloc`) still receive some degree of multithread efficiency. It's just that you can increase it further manually.

3. You can create your own handles and states with own configuration of block size, object size, and alignment. The global state contains handles that allocate powers of 2 only, so if your application allocates a lot of objects of size `192`, it might be worth to create your own handle/state with that as the `AllocSize` to improve cache friendliness and use less memory.

4. Fragmentation can be decreased by categorizing allocations into, for instance, frequency, lifetime, or (ideally) both. Objects of similar traits should be clumped together into the same state/handle. To give you a further idea of what this means, see [this](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkSystemAllocationScope.html) Vulkan manual page about allocation scope (i.e. lifetime). Since this is purely optional, it is not natively supported by the library. You can however implement simple inline functions that accept an `enum AllocType` and use that type to then index an array (`kAllocType` sized) of `AllocState` objects. After choosing one (i.e. appropriate for the allocation type), tail call a library function like usual, just that instead of using the global state, you will be using an explicit state (`S` function suffix).

5. The library is already largely suited to do that, but you can explicitly cache handles so that the library does not have to retrieve them from states upon every function call, given through benchmarking you notice that this is the bottleneck (very unlikely unless using custom state with a custom, possibly expensive, `AllocIndexFunc`).

## Standard API

To satisfy memory alignment guarantees for the global state, but at the same time keep an allocation size alongside the allocation to get rid of size fields in `AllocRealloc` and `AllocFree`, all memory allocations would have to be **AT LEAST** doubled in size. That is obviously not feasible and chances are that the default heap allocator, whatever it is, will perform better long-term.

Sadly, most software nowadays is suited towards the standard OG API that is not sized. Not much I can do about that. I wish I could use this allocator in Vulkan, or SDL, but oh well.

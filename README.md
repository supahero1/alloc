This is a custom memory allocator I wrote in C. Its features are:

1. Every code path is `O(1)`, no loops.

2. No per-allocation metadata, which allows small allocations to be precisely or closely what the user requested.

3. All allocations are always aligned at no additional cost to the next power of 2 of the size that is requested.

4. Thread-safe by default, with an option to disable it at compilation step.

5. Decentralized, allowing for scalability and fragmentation resistance.

6. Simpler and faster than all dlmalloc offsprings.

It supports Linux and Windows. It does not (and simply cannot) support WebAssembly.

## C

The allocator is written in C, so to build it, execute `make` in the root directory. That will create `bin/liballoc.so` as a debug build, with assertions all over the code. You can also run `RELEASE=1 make` to create a release build with no debug symbols. The allocator is not greately affected by a lack of optimizations (it's extremely simple), so it is recommended to go for a debug build, and only switch to release if necessary.

There are comments all over `src/alloc.c` that you can inspect for some initial knowledge of the allocator. Note that this is not your usual all-fits-one solution allocator - for once, you have to pass allocation size to deallocation functions.

For headers, `include/alloc_std.h` is the main one, containing all the functions that the library exposes. However, it is tedious to use the barebone library by itself, so there is also `include/alloc_ext.h` that contains a large set of inline functions that can help reduce the amount of code you write. That header does not define anything new and only uses functions available already in `include/alloc_std.h`.

Building the code by default will also test it. The test does not take long and also tests your default allocator (ptmalloc on linux distributions using glibc), jemalloc (if found), and mimalloc (if found), to make sure everything is consistent. There is also an option to benchmark it, but for that you have to `cd dev; make bench` by yourself. The benchmark is by default multithreaded. It used to be singlethreaded in a previous release, but nonetheless in both versions this allocator wins over dlmalloc.


This is a custom memory allocator I wrote in C. Its features are:

1. Every code path is `O(1)`, no loops.

2. No per-allocation metadata, which allows small allocations to be precisely or closely what the user requested.

3. All allocations are always aligned at no additional cost to the next power of 2 of the size that is requested.

4. Thread-safe by default, with an option to disable it at compilation step.

5. Decentralized, allowing for scalability and fragmentation resistance.

6. Simpler and faster than all dlmalloc offsprings. [^1]

[^1]: 33-50% faster on Linux, ~75% faster on Windows, but speed may vary between different environments and workloads.

It supports Linux and Windows (mingw/msys), 64bit and 32bit. It does not (and simply cannot) support WebAssembly.

A patch to support MacOS was already done, and it compiles, but linking issues are still there (I am not knowledgeable in `dylib` stuff).

## C

See [this](clang.md) for more information.

## C++

Dude I swear to god, I tried, but this is just not made for C++ is it.

## Rust

Rust exposes really cool `GlobalAlloc` API that can be overridden. After doing so, the language automatically uses the library for all allocations.

## Contributions

... are welcome. This is a very fresh, immature project, with little support for various platforms. It could use more updates.

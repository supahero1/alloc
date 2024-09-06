This is a custom memory allocator I wrote in C. Its features are:

1. Every code path is `O(1)`, no loops.

2. No per-allocation metadata, which allows small allocations to be precisely or closely what the user requested.

3. All allocations are always aligned at no additional cost to the next power of 2 of the size that is requested.

4. Thread-safe by default, with an option to disable it at compilation step.

5. Decentralized, allowing for scalability and fragmentation resistance.

6. Simpler, faster, using less kernel time than all dlmalloc offsprings. [^1]

[^1]: 33-50% faster on Linux/MacOS, ~75% faster on Windows, but speed may vary between different environments and workloads. The kernel time difference used to be clearly visible back when `time` was used to measure the benchmark, but there have been issues with the command not working on some platforms in specific settings, so it was replaced with in-code time measuring that only takes real time into account.

It supports Linux, MacOS, Windows (mingw/msys), both 64bit and 32bit. It does not (and simply cannot) support WebAssembly.

## C

See [this](clang.md) for more information.

## C++

Dude I swear to god, I tried, but this is just not made for C++ is it.

## Rust

Rust exposes really cool `GlobalAlloc` API that can be overridden. After doing so, the language automatically uses the library for all allocations.

## Contributions

... are welcome. This is a very fresh, immature projects. It could use more updates.

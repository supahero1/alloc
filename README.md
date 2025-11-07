This is a custom memory allocator I wrote in C. Its features are:

1. Lack of loops, where every memory-related code path is `O(1)`.

2. Lack of per-allocation metadata, increasing cache friendliness and decreasing memory footprint.

3. All allocations are by default aligned to the next or equal power of 2 of the size that is requested. [^2]

4. Thread-safe by default, with an option to disable it at compilation.

5. Decentralized, allowing for scalability and fragmentation resistance.

6. Simpler, faster, using less kernel time than all dlmalloc offsprings. [^1]

7. Completely encapsulated, with zero heap usage.

8. Customizable at compile-time (the defaults) as well as at runtime.

[^1]: 33-50% faster on Linux/MacOS, ~75% faster on Windows, but speed may vary between different environments and workloads. The kernel time difference used to be clearly visible back when `time` was used to measure the benchmark, but there have been issues with the command not working on some platforms in specific settings, so it was replaced with in-code time measuring that only takes real time into account.

[^2]: Zero cost for small allocations, then grows slightly as the allocation size grows, caps out at below a page size for large allocations (not per-allocation, but per-handle), which comparatively is rather little.

It supports Linux, MacOS, Windows (mingw/msys), 64bit and 32bit.

## C

See [this](clang.md) for more information.

## Rust

See [this](https://crates.io/crates/shalloc) for more information.

## WebAssembly

By design (no heap usage), WebAssembly is not and never will be supported.

## Docs

The first, older version of the code, had inline comments that you can still access [here](https://github.com/supahero1/alloc/blob/792cefe199ff2f6773e42519534fb7fe90ffe0bc/include/alloc_std.h). Not much has changed.

## Contributions

... are welcome. This is a very fresh, immature project. It could use more updates.

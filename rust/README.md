# shalloc

A Rust global allocator backed by [alloc](https://github.com/supahero1/alloc), a fast general-purpose allocator written in C.

```rs
#[global_allocator]
static ALLOCATOR: shalloc::Shalloc = shalloc::Shalloc;
```

## Performance

On x86, the allocator uses `lzcnt`, `tzcnt` and `popcnt` when the target has them, and portable fallbacks otherwise. To get the fast paths, build with a CPU that has them, for example:

```sh
RUSTFLAGS="-C target-cpu=native" cargo build --release
```

or `-C target-cpu=x86-64-v3` for portable binaries that run on most CPUs from the last decade.

The C code is always compiled with optimizations, even in debug builds of your crate.

## Notes

- Requires a C compiler supporting C23 (GCC 14+, Clang 18+).
- Supported platforms: Linux and Windows.
- Alignments above 4096 bytes are not supported. Such allocations fail, which aborts the program for standard collections.
- The allocator keeps sizable per-thread state in TLS with the initial-exec model, so a `cdylib` that is loaded at runtime with `dlopen` may fail to load. Executables are not affected.

See the [main repository](https://github.com/supahero1/alloc) for benchmarks and runtime configuration.

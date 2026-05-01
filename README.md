# alloc

This is the second generation of my memory allocator written in C. It's NUMA-aware and is highly competitive in terms of speed and memory usage with pt2malloc, jemalloc, mimalloc, tcmalloc, and tbbmalloc. If you run the benchmark, I would say there is a 50/50 chance my allocator is the fastest in the single-threaded and/or multi-threaded benchmarks. It is also pretty much guaranteed it is only getting beaten in memory usage by pt2malloc and tbbmalloc.

Do note that if I share any benchmark results below, they are **subjective** and only make sense when comparing them against each other when the error margin doesn't swallow up the difference. Furthermore, my allocator is NOT a general-purpose allocator like the ones I'm benchmarking it against, so there's that.

The benchmark suite consists of a number of precise tests, but also general mixed-workload ones. I was able to gain a lot of insight into how other allocators deal with specific situations, as well as identify areas in which I could improve. I make no guarantees that the benchmarks are fair or not flawed. In fact, in the past they were, and after adjusting them, my allocator turned out to be the worst out there, until I fixed its config.

An interesting example of what I noticed is that mimalloc suffers a lot from big allocations. I have not read its source code to know exactly what the problem is and have not used AI to do it for me, but its allocation cost is increasing linearly with the allocation size. My "good vibes" guess is that it seeks "mimalloc" pages (~64KiB if I'm not wrong) and glues them together to satisfy the request. This results in disastrous performance, especially given allocation sizes ~1MiB and higher, where mimalloc's p99.9 is usually 2.0-4.0ms, and mean is 0.2-0.4ms. In contrast, jemalloc, who is consistently the fastest in this situation, finalizes the request with a mean of 0.0014ms (around 300 times less!).

# Usage

Running `make install` will teleport `dev/alloc` into your `.local/bin`. That gives extreme quality of life, because that script gives you the ability to run the chaos test and do everything related to benchmarks easily. I tried experimenting with Makefile bindings for it and stuff, but it just doesn't feel the same.

The benchmarks are finalized in an `html` form and can be viewed in the browser or shared. In benchmark groups, like `ops` or `locality`, the script also generates pairwise comparisons.

Simply running `alloc` and help menus for its commands will tell you everything else that you need to know.

# Rust

If it's not outdated, see [this](https://crates.io/crates/shalloc).

# Tests

They are at the main repository [here](https://github.com/supahero1/monorepo/tree/master/tests/shared/alloc).

Furthermore, multiple times I hooked this allocator up to [Zed](https://zed.dev/), which allowed me to make further improvements and fixes.

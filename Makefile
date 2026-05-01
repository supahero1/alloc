.EXPORT_ALL_VARIABLES:

UNAME_S := $(shell uname -s)

COMMON_CFLAGS := -march=x86-64-v3 -Wall -Wextra -Wno-address-of-packed-member -D_GNU_SOURCE -Iinclude -fPIC -ftls-model=initial-exec
COMMON_LDLIBS := -latomic -pthread

ifeq ($(OS), Windows_NT)
LIB_EXT := dll
SHARED_LDFLAGS := -shared -Wl,--out-implib,bin/liballoc.lib
RPATH_FLAG :=
else
ifeq ($(UNAME_S), Darwin)
LIB_EXT := dylib
SHARED_LDFLAGS := -dynamiclib
RPATH_FLAG := -Wl,-rpath,@loader_path
else
LIB_EXT := so
SHARED_LDFLAGS := -shared
RPATH_FLAG := -Wl,-rpath,'$$ORIGIN'
endif
endif

LIB_RELEASE := bin/liballoc_release.$(LIB_EXT)
LIB_RELEASE_OBJDUMP := bin/liballoc_release_objdump.$(LIB_EXT)
LIB_DEBUG := bin/liballoc_debug.$(LIB_EXT)
LIB_DEV := bin/liballoc_dev.$(LIB_EXT)
LIB_INSTRUMENTED := bin/liballoc_instrumented.$(LIB_EXT)
LIB_DEFAULT := bin/liballoc.$(LIB_EXT)

RELEASE_CFLAGS := $(COMMON_CFLAGS) -fvisibility=hidden -O3 -flto -DNDEBUG
RELEASE_OBJDUMP_CFLAGS := $(COMMON_CFLAGS) -fvisibility=hidden -O3 -flto -DNDEBUG -g3 -ggdb
DEBUG_CFLAGS := $(COMMON_CFLAGS) -fvisibility=hidden -O1 -fno-lto -g3 -ggdb
DEV_LIB_CFLAGS := $(COMMON_CFLAGS) -fvisibility=default -O1 -fno-lto -g3 -ggdb -fno-omit-frame-pointer
INSTRUMENTED_CFLAGS := $(COMMON_CFLAGS) -fvisibility=hidden -O1 -fno-lto -g3 -ggdb -fno-omit-frame-pointer -DNDEBUG

DEV_CFLAGS := -march=x86-64-v3 -Wall -Wextra -Wno-address-of-packed-member -D_GNU_SOURCE -Iinclude
DEV_BFLAGS := $(DEV_CFLAGS) -pthread
BENCH_EXTRA_CFLAGS ?=

TEST_CHAOS_BIN := bin/test_chaos

BENCH_FNS_SRC := $(filter-out dev/bench_fns/common.c dev/bench_fns/app_mix.c, $(wildcard dev/bench_fns/*.c))
BENCH_BINS_OTHER := $(patsubst dev/bench_fns/%.c,bin/bench_detailed_other_%,$(BENCH_FNS_SRC))
BENCH_BINS_ALLOC := $(patsubst dev/bench_fns/%.c,bin/bench_detailed_alloc_%,$(BENCH_FNS_SRC))
BENCH_SUPPORT_OBJS := bin/platform.o bin/threads.o bin/debug.o bin/log.o bin/sync.o

.PHONY: all
all: libs chaos_build build_dev_bench

.PHONY: libs
libs: $(LIB_DEFAULT) $(LIB_RELEASE) $(LIB_RELEASE_OBJDUMP) $(LIB_DEBUG) $(LIB_DEV)

.PHONY: liballoc_release
liballoc_release: $(LIB_RELEASE)

.PHONY: liballoc_release_objdump
liballoc_release_objdump: $(LIB_RELEASE_OBJDUMP)

.PHONY: liballoc_debug
liballoc_debug: $(LIB_DEBUG)

.PHONY: liballoc_dev
liballoc_dev: $(LIB_DEV)

.PHONY: liballoc_instrumented
liballoc_instrumented: $(LIB_INSTRUMENTED)

.PHONY: build_dev_bench
build_dev_bench: $(BENCH_BINS_OTHER) $(BENCH_BINS_ALLOC)

.PHONY: chaos_build
chaos_build: $(LIB_DEV) $(TEST_CHAOS_BIN)

.PHONY: bench_detailed
bench_detailed: build_dev_bench

bin:
	mkdir -p bin

.PHONY: install
install:
	./dev/alloc install --name alloc

.PHONY: clean
clean:
	$(RM) -r bin

SRC_FILES := $(filter-out src/windows.c src/linux.c,$(wildcard src/*.c))

$(LIB_RELEASE): $(SRC_FILES) | bin
	$(CC) $(SHARED_LDFLAGS) $(RELEASE_CFLAGS) -o $@ $^ $(COMMON_LDLIBS)

$(LIB_RELEASE_OBJDUMP): $(SRC_FILES) | bin
	$(CC) $(SHARED_LDFLAGS) $(RELEASE_OBJDUMP_CFLAGS) -o $@ $^ $(COMMON_LDLIBS)

$(LIB_DEBUG): $(SRC_FILES) | bin
	$(CC) $(SHARED_LDFLAGS) $(DEBUG_CFLAGS) -o $@ $^ $(COMMON_LDLIBS)

$(LIB_DEV): $(SRC_FILES) | bin
	$(CC) $(SHARED_LDFLAGS) $(DEV_LIB_CFLAGS) -o $@ $^ $(COMMON_LDLIBS)

$(LIB_INSTRUMENTED): $(SRC_FILES) | bin
	$(CC) $(SHARED_LDFLAGS) $(INSTRUMENTED_CFLAGS) -o $@ $^ $(COMMON_LDLIBS)

$(LIB_DEFAULT): $(LIB_RELEASE)
	cp -f $< $@

$(TEST_CHAOS_BIN): dev/test_chaos.c $(LIB_DEV) | bin
	$(CC) $< -o $@ $(DEV_BFLAGS) -rdynamic -fno-omit-frame-pointer -fno-lto -g -UNDEBUG -Lbin -lalloc_dev $(RPATH_FLAG) $(COMMON_LDLIBS)

bin/bench_detailed_other_%: dev/bench_fns/%.c dev/bench_fns/common.c dev/bench_fns/app_mix.c $(BENCH_SUPPORT_OBJS) $(LIB_RELEASE) | bin
	$(CC) $(filter-out $(LIB_RELEASE), $^) -o $@ $(DEV_BFLAGS) $(BENCH_EXTRA_CFLAGS) -Lbin -lalloc_release $(RPATH_FLAG) $(COMMON_LDLIBS) -lm

bin/bench_detailed_alloc_%: dev/bench_fns/%.c dev/bench_fns/common.c dev/bench_fns/app_mix.c $(BENCH_SUPPORT_OBJS) $(LIB_RELEASE) | bin
	$(CC) $(filter-out $(LIB_RELEASE), $^) -DDEV_ALLOC -o $@ $(DEV_BFLAGS) $(BENCH_EXTRA_CFLAGS) -Lbin -lalloc_release $(RPATH_FLAG) $(COMMON_LDLIBS) -lm

bin/platform.o: src/platform.c | bin
	$(CC) $(RELEASE_CFLAGS) -c $< -o $@

bin/threads.o: src/threads.c | bin
	$(CC) $(RELEASE_CFLAGS) -c $< -o $@

bin/debug.o: src/debug.c | bin
	$(CC) $(RELEASE_CFLAGS) -c $< -o $@

bin/log.o: src/log.c | bin
	$(CC) $(RELEASE_CFLAGS) -c $< -o $@

bin/sync.o: src/sync.c | bin
	$(CC) $(RELEASE_CFLAGS) -c $< -o $@

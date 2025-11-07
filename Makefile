.EXPORT_ALL_VARIABLES:

CFLAGS := -Wall -Wextra -Wno-address-of-packed-member

ifeq ($(M32), 1)
CFLAGS += -m32
endif

ifeq ($(RELEASE), 1)
CFLAGS += -O3 -DNDEBUG
else
CFLAGS += -O0 -g3 -ggdb
endif

ifeq ($(OS), Windows_NT)
OUTPUT := bin/liballoc.dll
CFLAGS += -Wl,--out-implib,bin/liballoc.lib
else

ifeq ($(shell uname), Darwin)
OUTPUT := bin/liballoc.dylib
LDFLAGS := -dynamiclib
else
OUTPUT := bin/liballoc.so
LDFLAGS := -shared -fPIC
endif

endif

.PHONY: all
all: test

.PHONY: test
test: $(OUTPUT)
	cd dev; $(MAKE) test

.PHONY: bench
bench: $(OUTPUT)
	cd dev; $(MAKE) bench

bin:
	mkdir bin

.PHONY: clean
clean:
	$(RM) -r bin

$(OUTPUT): src/alloc.c src/debug.c src/sync.c | bin
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^

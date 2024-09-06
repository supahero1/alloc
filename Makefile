CFLAGS = -Wall -Wextra -Wno-address-of-packed-member

ifeq ($(RELEASE), 1)
CFLAGS += -O3 -DNDEBUG
else
CFLAGS += -O0 -g3 -ggdb
endif

ifeq ($(OS), Windows_NT)
OUTPUT := bin/liballoc.dll
CFLAGS += -Wl,--out-implib,bin/liballoc.lib
else
OUTPUT := bin/liballoc.so
endif

.PHONY: all
all: $(OUTPUT)
	$(MAKE) -C dev

bin:
	mkdir bin

.PHONY: clean
clean:
	$(RM) -r bin

$(OUTPUT): src/alloc.c src/debug.c | bin
	gcc -shared -fPIC $(CFLAGS) -o $@ $^

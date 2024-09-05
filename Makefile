CFLAGS = -Wall -Wextra -Werror -Wno-address-of-packed-member

ifeq ($(RELEASE), 1)
CFLAGS += -O3 -DNDEBUG
else
CFLAGS += -O0 -g3 -ggdb
endif

.PHONY: all
all: bin/liballoc.so
	$(MAKE) -C dev

bin:
	mkdir bin

.PHONY: clean
clean:
	$(RM) -r bin

bin/liballoc.so: src/alloc.c src/debug.c | bin
	gcc -shared -fPIC $(CFLAGS) -o $@ $^

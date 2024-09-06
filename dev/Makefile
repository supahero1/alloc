.PHONY: test
test:
	$(CC) test.c -o ../bin/test_other $(CFLAGS)
	$(CC) test.c -DDEV_ALLOC -o ../bin/test_alloc $(CFLAGS) -L../bin -lalloc

	../bin/test_other

	LD_PRELOAD=$(shell find /usr -name "libjemalloc.so") \
	../bin/test_other

	LD_PRELOAD=$(shell find /usr -name "libmimalloc.so") \
	../bin/test_other

	LD_LIBRARY_PATH=../bin:$$LD_LIBRARY_PATH \
	DYLD_LIBRARY_PATH=../bin:$$DYLD_LIBRARY_PATH \
	../bin/test_alloc


BFLAGS := $(CFLAGS) -pthread

.PHONY: bench
bench:
	$(CC) bench.c -o ../bin/bench_other $(BFLAGS)
	$(CC) bench.c -DDEV_ALLOC -o ../bin/bench_alloc $(BFLAGS) -L../bin -lalloc

	$(RM) ../bin/log.txt
	touch ../bin/log.txt
	echo "\nResults:" >> ../bin/log.txt

	echo "\n> pt2malloc:" >> ../bin/log.txt
	../bin/bench_other 2>> ../bin/log.txt

	echo "\n> jemalloc:" >> ../bin/log.txt
	LD_PRELOAD=$(shell find /usr -name "libjemalloc.so") \
	../bin/bench_other 2>> ../bin/log.txt

	echo "\n> mimalloc:" >> ../bin/log.txt
	LD_PRELOAD=$(shell find /usr -name "libmimalloc.so") \
	../bin/bench_other 2>> ../bin/log.txt

	echo "\n> alloc:" >> ../bin/log.txt
	LD_LIBRARY_PATH=../bin:$$LD_LIBRARY_PATH \
	DYLD_LIBRARY_PATH=../bin:$$DYLD_LIBRARY_PATH \
	../bin/bench_alloc 2>> ../bin/log.txt

	cat ../bin/log.txt

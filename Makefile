.PHONY: all
all: main
	./main

main: main.c alloc.c Makefile
	gcc -o main main.c alloc.c -O3 -Wall -Wextra

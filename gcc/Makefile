# BAD Makefile

CC := arm-none-linux-gnueabi-gcc
all:
	$(CC) -Wall -Wextra -isystem $$($(CC) -print-file-name=plugin)/include -fPIC -shared -O2 lockdep.c -o lockdep.so

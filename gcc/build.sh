CC=arm-none-linux-gnueabi-gcc

$CC -Wall -Wextra -isystem $(gcc -print-file-name=plugin)/include -fPIC -shared -O2 lockdep.c -o lockdep.so

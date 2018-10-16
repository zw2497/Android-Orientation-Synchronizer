CC = $(NDK_PATH)/clang
LD = $(NDK_PATH)/clang

########## YOUR MAKEFILE BELOW ##########
CFLAGS := -g -O2 -Wall -Werror

all: test1 test2

test1: test1.c
	$(CC) $(CFLAGS) -o test1 test1.c 

test2: test2.c
	$(CC) $(CFLAGS) -o test2 test2.c 

.PHONY: clean

clean:
	rm -f test1 test2

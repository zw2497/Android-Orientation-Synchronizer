CC = $(NDK_PATH)/clang
LD = $(NDK_PATH)/clang

########## YOUR MAKEFILE BELOW ##########
CFLAGS := -g -O2 -Wall -Werror

test: test.c
	$(CC) $(CFLAGS) -o test test.c 

.PHONY: clean

clean:
	rm -f test

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -g

.PHONY: all clean

all: pkg

pkg: pkg.c
	$(CC) $(CFLAGS) -o pkg pkg.c

clean:
	rm -f pkg

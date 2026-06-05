CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -D_GNU_SOURCE -I../../include
LDFLAGS =

ARGTABLE_DIR = ../../vendor/argtable3
ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.c
ARGTABLE_INC = -I$(ARGTABLE_DIR)

.PHONY: all clean

all: fetch libfetch.a

fetch: fetch_main.o cmd_fetch.o argtable3.o
	$(CC) $(LDFLAGS) -o $@ $^ -lm

libfetch.a: cmd_fetch.o argtable3.o
	ar rcs $@ $^

fetch_main.o: fetch_main.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

cmd_fetch.o: cmd_fetch.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

argtable3.o: $(ARGTABLE_SRC)
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

clean:
	rm -f fetch libfetch.a *.o

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -D_GNU_SOURCE -I../../include
LDFLAGS =

ARGTABLE_DIR = ../../vendor/argtable3
ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.c
ARGTABLE_INC = -I$(ARGTABLE_DIR)

.PHONY: all clean

all: tail libtail.a

tail: tail_main.o cmd_tail.o argtable3.o
	$(CC) $(LDFLAGS) -o $@ $^ -lm

libtail.a: cmd_tail.o argtable3.o
	ar rcs $@ $^

tail_main.o: tail_main.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

cmd_tail.o: cmd_tail.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

argtable3.o: $(ARGTABLE_SRC)
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

clean:
	rm -f tail libtail.a *.o

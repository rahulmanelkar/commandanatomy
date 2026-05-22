CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -D_GNU_SOURCE -I../../include
LDFLAGS =

ARGTABLE_DIR = ../../vendor/argtable3
ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.c
ARGTABLE_INC = -I$(ARGTABLE_DIR)

.PHONY: all clean

all: head libhead.a

head: head_main.o cmd_head.o argtable3.o
	$(CC) $(LDFLAGS) -o $@ $^ -lm

libhead.a: cmd_head.o argtable3.o
	ar rcs $@ $^

head_main.o: head_main.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

cmd_head.o: cmd_head.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

argtable3.o: $(ARGTABLE_SRC)
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

clean:
	rm -f head libhead.a *.o

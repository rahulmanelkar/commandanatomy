CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -D_GNU_SOURCE -I../../include
LDFLAGS =

ARGTABLE_DIR = ../../vendor/argtable3
ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.c
ARGTABLE_INC = -I$(ARGTABLE_DIR)

.PHONY: all clean

all: grep libgrep.a

grep: grep_main.o cmd_grep.o argtable3.o
	$(CC) $(LDFLAGS) -o $@ $^ -lm

libgrep.a: cmd_grep.o argtable3.o
	ar rcs $@ $^

grep_main.o: grep_main.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

cmd_grep.o: cmd_grep.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

argtable3.o: $(ARGTABLE_SRC)
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

clean:
	rm -f grep libgrep.a *.o

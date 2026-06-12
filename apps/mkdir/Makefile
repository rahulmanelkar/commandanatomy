CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -D_GNU_SOURCE -I../../include
LDFLAGS =

ARGTABLE_DIR = ../../vendor/argtable3
ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.c
ARGTABLE_INC = -I$(ARGTABLE_DIR)

.PHONY: all clean

all: mkdir libmkdir.a

mkdir: mkdir_main.o cmd_mkdir.o argtable3.o
	$(CC) $(LDFLAGS) -o $@ $^ -lm

libmkdir.a: cmd_mkdir.o argtable3.o
	ar rcs $@ $^

mkdir_main.o: mkdir_main.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

cmd_mkdir.o: cmd_mkdir.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

argtable3.o: $(ARGTABLE_SRC)
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

clean:
	rm -f mkdir libmkdir.a *.o

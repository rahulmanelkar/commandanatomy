CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -D_GNU_SOURCE -I../../include
LDFLAGS =

ARGTABLE_DIR = ../../vendor/argtable3
ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.c
ARGTABLE_INC = -I$(ARGTABLE_DIR)

.PHONY: all clean

all: hello libhello.a

hello: hello_main.o cmd_hello.o registry.o argtable3.o
	$(CC) $(LDFLAGS) -o $@ $^ -lm

libhello.a: cmd_hello.o registry.o argtable3.o
	ar rcs $@ $^

hello_main.o: hello_main.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

cmd_hello.o: cmd_hello.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

registry.o: registry.c
	$(CC) $(CFLAGS) -c -o $@ $<

argtable3.o: $(ARGTABLE_SRC)
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

clean:
	rm -f hello libhello.a *.o

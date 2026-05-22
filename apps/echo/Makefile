CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -D_GNU_SOURCE -I../../include
LDFLAGS =

ARGTABLE_DIR = ../../vendor/argtable3
ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.c
ARGTABLE_INC = -I$(ARGTABLE_DIR)

.PHONY: all clean

all: echo libecho.a

echo: echo_main.o cmd_echo.o argtable3.o
	$(CC) $(LDFLAGS) -o $@ $^ -lm

libecho.a: cmd_echo.o argtable3.o
	ar rcs $@ $^

echo_main.o: echo_main.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

cmd_echo.o: cmd_echo.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

argtable3.o: $(ARGTABLE_SRC)
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

clean:
	rm -f echo libecho.a *.o

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -D_GNU_SOURCE -I../../include
LDFLAGS =
ARGTABLE_DIR = ../../vendor/argtable3
ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.c
ARGTABLE_INC = -I$(ARGTABLE_DIR)

.PHONY: all clean

all: pkg libpkg.a

pkg: pkg_main.o cmd_pkg.o argtable3.o
	$(CC) $(LDFLAGS) -o $@ $^ -lm

libpkg.a: cmd_pkg.o argtable3.o
	ar rcs $@ $^

pkg_main.o: pkg_main.c
	$(CC) $(CFLAGS) -c -o $@ $<

cmd_pkg.o: cmd_pkg.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

argtable3.o: $(ARGTABLE_SRC)
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

clean:
	rm -f pkg libpkg.a pkg_main.o cmd_pkg.o argtable3.o

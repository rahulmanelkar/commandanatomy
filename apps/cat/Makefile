CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -D_GNU_SOURCE -I../../include
LDFLAGS =

ARGTABLE_DIR = ../../vendor/argtable3
ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.c
ARGTABLE_INC = -I$(ARGTABLE_DIR)

.PHONY: all clean

all: cat libcat.a

cat: cat_main.o cmd_cat.o argtable3.o
	$(CC) $(LDFLAGS) -o $@ $^ -lm

libcat.a: cmd_cat.o argtable3.o
	ar rcs $@ $^

cat_main.o: cat_main.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

cmd_cat.o: cmd_cat.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

argtable3.o: $(ARGTABLE_SRC)
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

clean:
	rm -f cat libcat.a *.o

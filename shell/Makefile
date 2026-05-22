CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -D_GNU_SOURCE -I../include
LDFLAGS = -lm

ARGTABLE_DIR = ../vendor/argtable3
ARGTABLE_INC = -I$(ARGTABLE_DIR)
ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.c

# Link cmd_*.o directly (one argtable3.o avoids duplicate symbols from .a files)
APP_OBJS = ../apps/hello/cmd_hello.o \
           ../apps/ls/cmd_ls.o       \
           ../apps/stat/cmd_stat.o   \
           ../apps/wc/cmd_wc.o       \
           ../apps/cat/cmd_cat.o

.PHONY: all clean apps

all: apps mysh

apps:
	$(MAKE) -C ../apps/hello
	$(MAKE) -C ../apps/ls
	$(MAKE) -C ../apps/stat
	$(MAKE) -C ../apps/wc
	$(MAKE) -C ../apps/cat

mysh: mysh.o tok.o argtable3.o $(APP_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

mysh.o: mysh.c tok.h
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

tok.o: tok.c tok.h
	$(CC) $(CFLAGS) -c -o $@ $<

argtable3.o: $(ARGTABLE_SRC)
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

clean:
	rm -f mysh mysh.o tok.o argtable3.o

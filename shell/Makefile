CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -D_GNU_SOURCE -I../include
LDFLAGS = -lm -lpthread

ARGTABLE_DIR = ../vendor/argtable3
ARGTABLE_INC = -I$(ARGTABLE_DIR)
ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.c

# jsmn: header-only JSON parser. Compiled to a single jsmn.o here (this is the
# one TU that pulls in its implementation); every other source includes jsmn.h
# with JSMN_HEADER defined, so there are no duplicate symbols — same discipline
# as argtable3.o above.
JSMN_DIR = ../vendor/jsmn
JSMN_INC = -I$(JSMN_DIR)
JSMN_SRC = $(JSMN_DIR)/jsmn.c

# Link cmd_*.o directly (one argtable3.o avoids duplicate symbols from .a files)
APP_OBJS = ../apps/hello/cmd_hello.o \
           ../apps/ls/cmd_ls.o       \
           ../apps/stat/cmd_stat.o   \
           ../apps/wc/cmd_wc.o       \
           ../apps/cat/cmd_cat.o     \
           ../apps/echo/cmd_echo.o   \
           ../apps/head/cmd_head.o   \
           ../apps/tail/cmd_tail.o   \
           ../apps/grep/cmd_grep.o   \
           ../apps/sort/cmd_sort.o   \
           ../apps/uniq/cmd_uniq.o   \
           ../apps/cut/cmd_cut.o     \
           ../apps/tee/cmd_tee.o     \
           ../apps/pkg/cmd_pkg.o     \
           ../apps/fetch/cmd_fetch.o \
           ../apps/mkdir/cmd_mkdir.o \
           ../apps/ftpd/cmd_ftpd.o

.PHONY: all clean apps

all: apps mysh

apps:
	$(MAKE) -C ../apps/hello
	$(MAKE) -C ../apps/ls
	$(MAKE) -C ../apps/stat
	$(MAKE) -C ../apps/wc
	$(MAKE) -C ../apps/cat
	$(MAKE) -C ../apps/echo
	$(MAKE) -C ../apps/head
	$(MAKE) -C ../apps/tail
	$(MAKE) -C ../apps/grep
	$(MAKE) -C ../apps/sort
	$(MAKE) -C ../apps/uniq
	$(MAKE) -C ../apps/cut
	$(MAKE) -C ../apps/tee
	$(MAKE) -C ../apps/pkg
	$(MAKE) -C ../apps/fetch
	$(MAKE) -C ../apps/mkdir
	$(MAKE) -C ../apps/ftpd

mysh: mysh.o tok.o linedit.o argtable3.o jsmn.o $(APP_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

mysh.o: mysh.c tok.h linedit.h
	$(CC) $(CFLAGS) $(ARGTABLE_INC) $(JSMN_INC) -c -o $@ $<

tok.o: tok.c tok.h
	$(CC) $(CFLAGS) -c -o $@ $<

linedit.o: linedit.c linedit.h
	$(CC) $(CFLAGS) -c -o $@ $<

argtable3.o: $(ARGTABLE_SRC)
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

jsmn.o: $(JSMN_SRC)
	$(CC) $(CFLAGS) $(JSMN_INC) -c -o $@ $<

clean:
	rm -f mysh mysh.o tok.o linedit.o argtable3.o jsmn.o

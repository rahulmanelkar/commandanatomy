CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -D_GNU_SOURCE -I../../include
LDFLAGS = -lpthread

ARGTABLE_DIR = ../../vendor/argtable3
ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.c
ARGTABLE_INC = -I$(ARGTABLE_DIR)

.PHONY: all clean

all: ftpd libftpd.a

# The daemon reuses the ls and mkdir command specs by reference, so the
# standalone binary links their objects (compiled here so the binary is
# self-contained regardless of sub-make build order).
ftpd: ftpd_main.o cmd_ftpd.o cmd_ls.o cmd_mkdir.o argtable3.o
	$(CC) -o $@ $^ $(LDFLAGS) -lm

libftpd.a: cmd_ftpd.o
	ar rcs $@ $^

ftpd_main.o: ftpd_main.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

cmd_ftpd.o: cmd_ftpd.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

cmd_ls.o: ../ls/cmd_ls.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

cmd_mkdir.o: ../mkdir/cmd_mkdir.c
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

argtable3.o: $(ARGTABLE_SRC)
	$(CC) $(CFLAGS) $(ARGTABLE_INC) -c -o $@ $<

clean:
	rm -f ftpd libftpd.a *.o

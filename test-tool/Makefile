LIBS=-lpopt 
CC=gcc
CFLAGS=-g -O0 -fPIC -Wall -W -I. -I./include -I../include "-D_U_=__attribute__((unused))"
TESTS=0100_read10_simple.o 0101_read10_beyond_eol.o 0102_read10_0blocks.o \
0103_read10_rdprotect.o 0104_read10_flags.o 0105_read10_invalid.o \
0110_readcapacity10_simple.o 0111_readcapacity10_pmi.o \
0120_read6_simple.o

all: iscsi-test

iscsi-test: iscsi-test.c $(TESTS)
	$(CC) $(CFLAGS) iscsi-test.c -o iscsi-test $(TESTS) ../lib/libiscsi.a $(LIBS)

clean:
	rm -f *.o
	rm -f iscsi-test
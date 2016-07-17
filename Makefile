CFLAGS=-g -O1 -Wall -Wno-switch -Wextra -fstack-protector-strong -D_FORTIFY_SOURCE=2

ALL = scan thread hdr show list unmime mseq

all: $(ALL)

scan: blaze822.o scan.o rfc2047.o
thread: blaze822.o thread.o
hdr: blaze822.o hdr.o rfc2047.o
show: blaze822.o show.o rfc2045.o rfc2047.c
list: list.o
unmime: blaze822.o unmime.o rfc2045.o rfc2047.o
mseq: mseq.o seq.o

clean: FRC
	-rm -f $(ALL) *.o

FRC:

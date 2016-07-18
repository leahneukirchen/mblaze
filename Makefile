CFLAGS=-g -O1 -Wall -Wno-switch -Wextra -fstack-protector-strong -D_FORTIFY_SOURCE=2

ALL = scan thread hdr show list mseq

all: $(ALL)

scan: scan.o blaze822.o seq.o rfc2047.o
thread: thread.o blaze822.o seq.o
hdr: hdr.o blaze822.o seq.o rfc2047.o
show: show.o blaze822.o seq.o rfc2045.o rfc2047.c
list: list.o
mseq: mseq.o seq.o

clean: FRC
	-rm -f $(ALL) *.o

FRC:

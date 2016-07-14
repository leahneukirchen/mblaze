CFLAGS=-g -O1 -Wall -Wno-switch -Wextra -fstack-protector-strong -D_FORTIFY_SOURCE=2

ALL = scan thread hdr show list next unmime

all: $(ALL)

scan: blaze822.o scan.o rfc2047.o
thread: blaze822.o thread.o
hdr: blaze822.o hdr.o
show: blaze822.o show.o
list: list.o
next: next.o
unmime: blaze822.o unmime.o rfc2045.o rfc2047.o

clean: FRC
	-rm -f $(ALL) *.o

FRC:

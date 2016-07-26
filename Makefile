CFLAGS=-g -O1 -Wall -Wno-switch -Wextra -fstack-protector-strong -D_FORTIFY_SOURCE=2

ALL = maddr mdeliver mdirs mflag mhdr minc mlist mmime mscan mseq mshow msort mthread

all: $(ALL)

maddr: maddr.o blaze822.o seq.o rfc2047.o mymemmem.o
mdeliver: mdeliver.o blaze822.o mymemmem.o
mdirs: mdirs.o
mflag: mflag.o blaze822.o seq.o mymemmem.o
mhdr: mhdr.o blaze822.o seq.o rfc2047.o mymemmem.o
minc: minc.o
mlist: mlist.o
mmime: mmime.o
mscan: mscan.o blaze822.o seq.o rfc2047.o mymemmem.o
mseq: mseq.o seq.o
mshow: mshow.o blaze822.o seq.o rfc2045.o rfc2047.c mymemmem.o
msort: msort.o blaze822.o seq.o mystrverscmp.o mymemmem.o
mthread: mthread.o blaze822.o seq.o mymemmem.o

README: man/mintro.7
	mandoc -Tutf8 $< | col -bx >$@

clean: FRC
	-rm -f $(ALL) *.o

FRC:

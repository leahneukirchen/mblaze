CFLAGS=-g -O1 -Wall -Wno-switch -Wextra -fstack-protector-strong -D_FORTIFY_SOURCE=2

ALL = mdirs mflag mhdr minc mlist mmime mscan mseq mshow msort mthread

all: $(ALL)

mdirs: mdirs.o
mflag: mflag.o blaze822.o seq.o
mhdr: mhdr.o blaze822.o seq.o rfc2047.o
minc: minc.o
mlist: mlist.o
mmime: mmime.o
mscan: mscan.o blaze822.o seq.o rfc2047.o
mseq: mseq.o seq.o
mshow: mshow.o blaze822.o seq.o rfc2045.o rfc2047.c
msort: msort.o blaze822.o seq.o
mthread: mthread.o blaze822.o seq.o

clean: FRC
	-rm -f $(ALL) *.o

FRC:

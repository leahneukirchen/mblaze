CFLAGS=-g -O1 -Wall -Wno-switch -Wextra -fstack-protector-strong -D_FORTIFY_SOURCE=2

ALL = mscan mthread mhdr mshow mlist mseq msort mmime minc mflag

all: $(ALL)

mscan: mscan.o blaze822.o seq.o rfc2047.o
mthread: mthread.o blaze822.o seq.o
mhdr: mhdr.o blaze822.o seq.o rfc2047.o
mshow: mshow.o blaze822.o seq.o rfc2045.o rfc2047.c
mlist: mlist.o
mseq: mseq.o seq.o
msort: msort.o blaze822.o seq.o
mmime: mmime.o
minc: minc.o
mflag: mflag.o blaze822.o seq.o

clean: FRC
	-rm -f $(ALL) *.o

FRC:

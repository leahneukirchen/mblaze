CFLAGS=-g -O2 -Wall -Wno-switch -Wextra -fstack-protector-strong -D_FORTIFY_SOURCE=2

DESTDIR=
PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man

ALL = maddr magrep mdate mdeliver mdirs mflag mgenmid mhdr minc mlist mmime mpick mscan msed mseq mshow msort mthread
SCRIPT = mcolor mcomp mless mquote

all: $(ALL)

maddr: maddr.o blaze822.o seq.o rfc2047.o mymemmem.o
magrep: magrep.o blaze822.o seq.o rfc2045.o rfc2047.o mymemmem.o
mdate: mdate.o
mdeliver: mdeliver.o blaze822.o mymemmem.o
mdirs: mdirs.o
mflag: mflag.o blaze822.o seq.o mymemmem.o
mgenmid: mgenmid.o blaze822.o seq.o mymemmem.o
mhdr: mhdr.o blaze822.o seq.o rfc2047.o mymemmem.o
minc: minc.o
mlist: mlist.o
mmime: mmime.o
mpick: mpick.o blaze822.o seq.o rfc2047.c mymemmem.o
mscan: mscan.o blaze822.o seq.o rfc2047.o mymemmem.o
msed: msed.o blaze822.o seq.o mymemmem.o
mseq: mseq.o seq.o
mshow: mshow.o blaze822.o seq.o rfc2045.o rfc2047.c mymemmem.o filter.o
msort: msort.o blaze822.o seq.o mystrverscmp.o mymemmem.o
mthread: mthread.o blaze822.o seq.o mymemmem.o

README: man/mblaze.7
	mandoc -Tutf8 $< | col -bx >$@

clean: FRC
	-rm -f $(ALL) *.o

install: FRC all
	mkdir -p $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)/man1
	install -m0755 $(ALL) $(SCRIPT) $(DESTDIR)$(BINDIR)
	ln -sf mless $(DESTDIR)$(BINDIR)/mnext
	ln -sf mless $(DESTDIR)$(BINDIR)/mprev
	ln -sf mrepl $(DESTDIR)$(BINDIR)/mcomp
	install -m0644 man/*.[0-9] $(DESTDIR)$(MANDIR)/man1

FRC:

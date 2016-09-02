CFLAGS+=-g -O2 -Wall -Wno-switch -Wextra -fstack-protector-strong -D_FORTIFY_SOURCE=2

DESTDIR=
PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man

ALL = maddr magrep mdate mdeliver mdirs mexport mflag mgenmid mhdr minc mlist mmime mpick mscan msed mseq mshow msort mthread
SCRIPT = mcolor mcom mless mquote

all: $(ALL)

$(ALL) : % : %.o
maddr magrep mdeliver mexport mflag mgenmid mhdr mpick mscan msed mshow \
  msort mthread : blaze822.o mymemmem.o mytimegm.o
maddr magrep mexport mflag mgenmid mhdr mpick mscan msed mseq mshow msort \
  mthread : seq.o
maddr magrep mhdr mpick mscan mshow : rfc2047.o
magrep mshow : rfc2045.o
mshow : filter.o
msort : mystrverscmp.o

README: man/mblaze.7
	mandoc -Tutf8 $< | col -bx >$@

clean: FRC
	-rm -f $(ALL) *.o

install: FRC all
	mkdir -p $(DESTDIR)$(BINDIR) \
		$(DESTDIR)$(MANDIR)/man1 \
		$(DESTDIR)$(MANDIR)/man7
	install -m0755 $(ALL) $(SCRIPT) $(DESTDIR)$(BINDIR)
	ln -sf mless $(DESTDIR)$(BINDIR)/mnext
	ln -sf mless $(DESTDIR)$(BINDIR)/mprev
	ln -sf mcom $(DESTDIR)$(BINDIR)/mrep
	install -m0644 man/*.1 $(DESTDIR)$(MANDIR)/man1
	install -m0644 man/*.7 $(DESTDIR)$(MANDIR)/man7

FRC:

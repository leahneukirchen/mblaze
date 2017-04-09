.error : This Makefile needs GNU make

CFLAGS+=-g -O2 -Wall -Wno-switch -Wextra -fstack-protector-strong -D_FORTIFY_SOURCE=2
LDLIBS=-lrt
ifdef OPENBSD
CFLAGS+=-I/usr/local/include
LDLIBS=-L/usr/local/lib -liconv
endif

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
maddr magrep mexport mflag mgenmid mhdr mlist mpick mscan msed mseq mshow msort \
  mthread : seq.o slurp.o
maddr magrep mhdr mpick mscan mshow : rfc2047.o
magrep mshow : rfc2045.o
mshow : filter.o safe_u8putstr.o rfc2231.o
msort : mystrverscmp.o
mmime : slurp.o

README: man/mblaze.7
	mandoc -Tutf8 $< | col -bx >$@

clean: FRC
	-rm -f $(ALL) *.o

check: FRC all
	PATH=$$(pwd):$$PATH prove -v

install: FRC all
	mkdir -p $(DESTDIR)$(BINDIR) \
		$(DESTDIR)$(MANDIR)/man1 \
		$(DESTDIR)$(MANDIR)/man7
	install -m0755 $(ALL) $(SCRIPT) $(DESTDIR)$(BINDIR)
	ln -sf mless $(DESTDIR)$(BINDIR)/mnext
	ln -sf mless $(DESTDIR)$(BINDIR)/mprev
	ln -sf mcom $(DESTDIR)$(BINDIR)/mrep
	install -m0644 man/*.1 $(DESTDIR)$(MANDIR)/man1
	install -m0644 man/*.5 $(DESTDIR)$(MANDIR)/man5
	install -m0644 man/*.7 $(DESTDIR)$(MANDIR)/man7

FRC:

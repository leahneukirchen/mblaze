CFLAGS+=-g -O2 -Wall -Wno-switch -Wextra -fstack-protector-strong -D_FORTIFY_SOURCE=2
LDLIBS=-lrt

OS := $(shell uname)

ifeq ($(OS),OpenBSD)
CFLAGS+=-I/usr/local/include -pthread
LDLIBS=-L/usr/local/lib -liconv -pthread
endif

ifeq ($(OS),Darwin)
LDLIBS=-liconv
endif

DESTDIR=
PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man

ALL = maddr magrep mdate mdeliver mdirs mexport mflag mgenmid mhdr minc mlist mmime mpick mscan msed mseq mshow msort mthread
SCRIPT = mcolor mcom mless mmkdir mquote museragent

all: $(ALL) museragent

$(ALL) : % : %.o
maddr magrep mdeliver mexport mflag mgenmid mhdr mpick mscan msed mshow \
  msort mthread : blaze822.o mymemmem.o mytimegm.o
maddr magrep mexport mflag mgenmid mhdr mlist mpick mscan msed mseq mshow msort \
  mthread : seq.o slurp.o
maddr magrep mhdr mpick mscan mshow : rfc2047.o
magrep mshow : rfc2045.o
mshow : filter.o safe_u8putstr.o rfc2231.o pipeto.o
mscan : pipeto.o
msort : mystrverscmp.o
mmime : slurp.o

museragent: FRC
	@printf '#!/bin/sh\nprintf "User-Agent: mblaze/%s (%s)\\n"\n' \
		"$$({ git describe --always --dirty 2>/dev/null || cat VERSION; } | sed 's/^v//')" \
		"$$(date +%Y-%m-%d)" >$@
	@chmod +x $@

README: man/mblaze.7
	mandoc -Tutf8 $< | col -bx >$@

clean: FRC
	-rm -f $(ALL) *.o museragent

check: FRC all
	PATH=$$(pwd):$$PATH prove -v

install: FRC all
	mkdir -p $(DESTDIR)$(BINDIR) \
		$(DESTDIR)$(MANDIR)/man1 \
		$(DESTDIR)$(MANDIR)/man5 \
		$(DESTDIR)$(MANDIR)/man7
	install -m0755 $(ALL) $(SCRIPT) $(DESTDIR)$(BINDIR)
	ln -sf mless $(DESTDIR)$(BINDIR)/mnext
	ln -sf mless $(DESTDIR)$(BINDIR)/mprev
	ln -sf mcom $(DESTDIR)$(BINDIR)/mfwd
	ln -sf mcom $(DESTDIR)$(BINDIR)/mrep
	install -m0644 man/*.1 $(DESTDIR)$(MANDIR)/man1
	install -m0644 man/*.5 $(DESTDIR)$(MANDIR)/man5
	install -m0644 man/*.7 $(DESTDIR)$(MANDIR)/man7

release:
	VERSION=$$(git describe --tags | sed 's/^v//;s/-[^.]*$$//') && \
	git archive --prefix=mblaze-$$VERSION/ -o mblaze-$$VERSION.tar.gz HEAD

sign:
	VERSION=$$(git describe --tags | sed 's/^v//;s/-[^.]*$$//') && \
	gpg --armor --detach-sign mblaze-$$VERSION.tar.gz && \
	signify -S -s ~/.signify/mblaze.sec -m mblaze-$$VERSION.tar.gz && \
	sed -i '1cuntrusted comment: verify with mblaze.pub' mblaze-$$VERSION.tar.gz.sig

FRC:

# Compiling and installing mblaze

You must use GNU make to build.

Use `make all` to build, `make install` to install relative to `PREFIX`
(`/usr/local` by default).  The `DESTDIR` convention is respected.

`mblaze` has been tested on
- Linux 4.7 (glibc 2.24)
- Linux 4.7 (musl 1.1.15)
- Linux 3.2 (glibc 2.13)
- FreeBSD 11.0
- OpenBSD 5.9

On OpenBSD, you must use `make OPENBSD=1` and provide `libiconv`.

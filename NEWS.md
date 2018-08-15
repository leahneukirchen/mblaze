## 0.4 (2018-08-15)

* New tool mrefile to move or copy messages.
* contrib/mp7m: add application/pkcs7-mime decoder
* mcom: allow setting arbitrary headers from command line
* mcom: add -body to prepopulate drafts
* mcom: mark drafts as seen after sending
* mcom: update flags after mrep/mbnc/mfwd
* mshow: add -B to decode MIME parameters in broken mails
* magrep: add -l flag
* Many bug fixes.

## 0.3.2 (2018-02-13)

* magrep: add *:REGEX to search in any header
* Fix of a buffer overflow in blaze822_multipart.
* Small bug fixes.
* Many documentation improvements by Larry Hynes.

## 0.3.1 (2018-01-30)

* mless: support $NO_COLOR
* mcolor: support $NO_COLOR
* blaze822.h: ensure PATH_MAX is defined
* Improved documentation.
* Many fixes for address parser.

## 0.3 (2018-01-12)

* New tool mflow to reformat format=flowed plain text mails.
* New tool mbnc to bounce mails (send original mail to someone else).
* mshow filters can output raw text now, e.g. for HTML rendering with colors.
* mrep can quote mail that doesn't have a plain text part.
* mcom runs mmime when deemed necessary.
* mhdr can extract MIME parameters.
* New contrib: mrecode
* New contrib: mraw
* mshow regards non-MIME mails as MIME mails with one part now.
* mshow -F to disable MIME filters.
* mpick supports negations now.
* msed can remove headers depending on their value.
* Improved UTF-8 parsing.
* Improved documentation.
* Numerous bug fixes and portability fixes.

## 0.2 (2017-07-17)

* New sequence syntax `m:+n` for `n` messages after message `m`.
* Threading shortcuts `=`, `_`, `^` for `.=`, `._`, `.^`.
* Sequence related errors are now reported.
* minc and mlist normalize slashes in paths.
* mfwd now generates conforming message/rfc822 parts.
* mthread can add optional folders (e.g. your outbox) to resolve message ids.
* mcom now adds Date: just before sending or cancelling the mail.
* VIOLATIONS.md documents how mblaze works with certain common mistakes.
* Full documentation revamp by Larry Hynes.
* Fix rare crash looking for mail body.
* Numerous small bug and portability fixes.

## 0.1 (2017-06-24)

* Initial release

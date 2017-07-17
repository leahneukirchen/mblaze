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

# Standard-violations detected in the wild during development of mblaze

This list is probably not complete.

* RFC5322 assumes CRLF line endings throughout, but Maildir messages
  are generally using Unix line endings.  mblaze accepts both, and
  only uses CRLF when required (e.g. for signing).

* Backslashes in atoms (RFC 5322, 3.2.3) are parsed as if they were
  inside quoted strings.

* Return-path is accepted without angle-addr (RFC5322, 3.6.7).

* Encoded words within quoted strings (RFC2047, 5.3) are decoded for
  header printing.

* Encoded words within MIME parameters (RFC2047, 5.3) are NOT decoded.

* Empty encoded words are decoded as empty string (RFC2047, 2).

* Split multi-octet characters between encoded words (RFC2047, 5.3)
  are reassembled if the encodings agree.

* Date parsing is strict, obsolete timezone and two-digit years are
  not parsed (RFC5322, 4.3).

* Mails without MIME-Version (RFC2045, 4) are still subject to
  MIME decoding if the Content-Transfer-Encoding header is present.

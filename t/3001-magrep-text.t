#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 2

rm -rf test.dir
mkdir test.dir
(
cd test.dir

mkdir -p "inbox/cur"

cat <<! >"inbox/cur/1:2,"
Subject: test with double attachment
Content-Type: multipart/related; boundary="000000000000931c780636c17d9b"
MIME-Version: 1.0

--000000000000931c780636c17d9b
Content-Type: multipart/alternative; boundary="000000000000931c780636c17d9a"

--000000000000931c780636c17d9a
Content-Type: text/plain; charset="UTF-8"

This is plain text.

--000000000000931c780636c17d9a--

--000000000000931c780636c17d9b
Content-Disposition: attachment; filename=binary.file
Content-Type: application/binary
Content-Transfer-Encoding: base64

N8yqvFLGLyQ9df3oWNxcL/QcY3n5ape10aMZBq3zA/uuO352xpr8O3/QBV2ptMcZbePiDUmDRFPT
GldqOO7ggqXYPUkZ+VpMqpO4F7kYFV9W8ns2W354O8UsovW8Dms0Hqlgr1qAYkOgvc+IewfVh+xa
hrYPMYJx5GeW9syYHmJA9Gjdttc0Z76uHoKPZ0hEamdh9lQnmO1HLdT6dU0gGkbiNPejr9S3SU3a
bBLCF2iuShN7PHlubfAgU4Ne+EnfeSkojTX+HXt54P3E2X/AqLYR7vJk7EAQ2/WOzMHE/glAdqAv
Gv70LvH/HKgjBK6JMeOe5ZdmSNd42IoOEEgJD7eFGrOouGaDsiZpLon7idadebqm/9AAQQGkN1xA
Z9Uu3UTyChxSLKKriULDwlkHYBM3M/nSBIJ2wm03A8ND4GWAoPpdWmRRoku0jjshnJbEN2Pt9RFI
C60HAoLxzghvdIoNPlf9F8iQ2nH4vaks0KUFgZWLiaOw4jFCDADtTOuIvpPO52u9qyy/alH9Rmd9
6+jRyuYQlFUo3olIjRz2OPiHo3Bm6cliTR6V8mEarCus01wr0Zb2bVbfmJ24VWcJ9HkRIFPiuC0m
fpq12fi5mVlET7MgDRBaUmdqj6R4XQZpNhv7GxslLvLf7hBJ6T0cBkYWbElf+C9zqSWXfy3zeDs=

--000000000000931c780636c17d9b--
!

export MAILSEQ=seq

check_test 'match in body' -eq 1 'magrep /:plain ./inbox/cur/1:2, | wc -l'
check_test 'not match inside attachment' -eq 0 'magrep /:jdttc ./inbox/cur/1:2, | wc -l'

)

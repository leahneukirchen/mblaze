#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 3

# Mail with \n\n and \r\n\r\n
cr=$(printf '\r')
cat <<EOF >tmp
Content-Type: multipart/form-data; boundary=------------------------55a586f81559face$cr
$cr
--------------------------55a586f81559face$cr
Content-Disposition: form-data; name="a"; filename="foo"$cr
Content-Type: application/octet-stream$cr
$cr
foo$cr

previously there are two NL$cr
$cr
--------------------------55a586f81559face$cr
Content-Disposition: form-data; name="a"; filename="bar"$cr
Content-Type: application/octet-stream$cr
$cr
bar$cr
$cr
--------------------------55a586f81559face--$cr
EOF

check 'mail has 3 attachments' 'mshow -t ./tmp | wc -l | grep 4'
check 'mail attachment foo has size 35' 'mshow -t ./tmp | grep size=35.*name=\"foo\"'

# Mail with linebreaks in base64 quartets
cat <<EOF >tmp
Subject: base64
MIME-Version: 1.0
Content-Type: multipart/mixed; boundary="----_=_2f8f1e2243b55f8618eaf0d9_=_"

This is a multipart message in MIME format.

------_=_2f8f1e2243b55f8618eaf0d9_=_
Content-Disposition: attachment; filename=base64
Content-Type: application/binary
Content-Transfer-Encoding: base64

dGhp
cyBpc
yBzb21
lIGJhc2
U2NAo=

------_=_2f8f1e2243b55f8618eaf0d9_=_--
EOF

check 'mail decodes correctly' 'mshow -O ./tmp 2 | grep -q "this is some base64"'

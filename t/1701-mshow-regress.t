#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 2

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

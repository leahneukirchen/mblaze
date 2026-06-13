#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 3

# Nested MIME where the outer boundary is a prefix of the inner boundary
cat <<EOF >tmp
MIME-Version: 1.0
Content-type: multipart/mixed; charset=iso-8859-1;
 boundary="_xec5AqfRYxfhARmklHx"


--_xec5AqfRYxfhARmklHx
Content-type: Multipart/alternative; charset=iso-8859-1;
 boundary="_xec5AqfRYxfhARmklHx8"


--_xec5AqfRYxfhARmklHx8
Content-type: text/plain; charset=iso-8859-1
Content-Transfer-Encoding: 7bit
Content-Disposition: inline

foo
--_xec5AqfRYxfhARmklHx8
Content-type: text/html; charset=iso-8859-1
Content-Transfer-Encoding: Quoted-printable
Content-Disposition: inline

bar
--_xec5AqfRYxfhARmklHx8--

--_xec5AqfRYxfhARmklHx
Content-Type: application/zip
Content-Transfer-Encoding: Base64

quux
--_xec5AqfRYxfhARmklHx--
EOF

check 'nested mail has 5 attachments' 'mshow -t ./tmp | wc -l | grep 6'
check 'nested mail has text/html attachment' 'mshow -t ./tmp | grep text/html'
check 'non-existent mail' 'mshow ./does-not-exist ; [ $? -eq 1 ]'

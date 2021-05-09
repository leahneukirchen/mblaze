#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 12

cat <<EOF >tmp
References: <aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa@a> <bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb@b> <ccccccccccccccccccccccccccccccc@c>

Body
EOF

# https://github.com/leahneukirchen/mblaze/issues/20

check 'mime -r runs' 'mmime -r <tmp >tmp2'
check 'no overlong lines' 'awk "{if(length(\$0)>=80)exit 1}" <tmp2'
check 'no QP when unecessary' ! grep -qF "=?" tmp2
check 'no further mime necessary' 'mmime -c <tmp2'

cat <<EOF >tmp2
Subject: inclusion test

#message/rfc822 $PWD/tmp
EOF

check 'include works' 'mmime <tmp2 | grep Body'
check 'include sets filename' 'mmime <tmp2 | grep filename=tmp'


cat <<EOF >tmp2
Subject: inclusion test no filename

#message/rfc822 $PWD/tmp>
EOF

check 'include works, overriding filename' 'mmime <tmp2 | grep Disposition | grep -v filename=tmp'


cat <<EOF >tmp2
Subject: inclusion test with other disposition

#message/rfc822#inline $PWD/tmp>
EOF

check 'include works, overriding filename' 'mmime <tmp2 | grep Disposition | grep inline'


cat <<EOF >tmp2
Subject: message with content-type
Content-Type: text/plain; format=flowed

This message has format-flowed.
EOF

check 'content-type is respected if found in input' 'mmime -r <tmp2 |grep format=flowed'

cat <<EOF >tmp2
Subject: message with content-transfer-encoding
Content-Transfer-Encoding: quoted-printable

This message has already encoded. f=C3=B6=C3=B6.
EOF


check 'content-transfer-encoding is respected if found in input' 'mmime -r <tmp2 |grep f=C3=B6=C3=B6'

cat <<EOF >tmp2
Subject: message with content-type
Content-Type: text/plain; format=flowed

This message has format-flowed.

#message/rfc822 $PWD/tmp

This part too.
EOF


check 'content-type is respected if found in input, for multipart/mixed' 'mmime <tmp2 |grep format=flowed'

cat <<EOF >tmp2
Subject: message with content-transfer-encoding
Content-Transfer-Encoding: Quoted-Printable

This message has already encoded. f=C3=B6=C3=B6.

#message/rfc822 $PWD/tmp

This part too.
EOF

check 'content-transfer-encoding is respected if found in input, for multipart/mixed' 'mmime <tmp2 |grep f=C3=B6=C3=B6'

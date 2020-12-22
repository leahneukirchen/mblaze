#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 8

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

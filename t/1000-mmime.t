#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 4

cat <<EOF >tmp
References: <aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa@a> <bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb@b> <ccccccccccccccccccccccccccccccc@c>

Body
EOF

# https://github.com/chneukirchen/mblaze/issues/20

check 'mime -r runs' 'mmime -r <tmp >tmp2'
check 'no overlong lines' '[ $(wc -L <tmp2) -lt 80 ]'
check 'no QP when unecessary' ! grep -qF "=?" tmp2
check 'no further mime necessary' 'mmime -c <tmp2'

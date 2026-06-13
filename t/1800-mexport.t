#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 3

cat <<EOF >tmp.1
Subject: message 1

This is message 1.
EOF

cat <<EOF >tmp.2
Subject: message 2

This is message 2.  It has a trailing empty line.

EOF

printf >tmp.3 'Subject: message 3

This is message 3.  It has a no trailing newline, oops.'

mexport ./tmp.1 ./tmp.2 ./tmp.3 >./tmp.mbox

check 'generated mbox has 16 lines' 'cat ./tmp.mbox | wc -l | grep 16'
check 'generated mbox has 7 empty lines' 'grep -c "^$" ./tmp.mbox | grep 7'
check 'non-existent mail' 'mexport ./foo-bar ; [ $? -eq 1 ]'

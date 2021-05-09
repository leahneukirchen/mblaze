#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 2

rm -rf test.dir
mkdir test.dir
cd test.dir

mmkdir inbox

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

cat <<EOF >tmp.4
Subject: message 4



This is message 4.  It has multiple trailing empty lines.


EOF

mexport ./tmp.1 | mdeliver -M inbox/
check 'message 1 is delivered verbatim via mbox' 'cmp ./tmp.1 ./inbox/new/*:2,'
rm -f ./inbox/new/*

mexport ./tmp.2 | mdeliver -M inbox/
check 'message 2 is delivered verbatim via mbox' 'cmp ./tmp.2 ./inbox/new/*:2,'
rm -f ./inbox/new/*

mexport ./tmp.3 | mdeliver -M inbox/
check 'message 3 gets a newline via mbox' 'awk 1 ./tmp.3 | cmp - ./inbox/new/*:2,'
rm -f ./inbox/new/*

mexport ./tmp.4 | mdeliver -M inbox/
check 'message 4 gets delivered verbatim via mbox' 'cmp ./tmp.4 ./inbox/new/*:2,'
rm -f ./inbox/new/*

mdeliver inbox/ <./tmp.1
check 'message 1 is delivered verbatim via stdin' 'cmp ./tmp.1 ./inbox/new/*:2,'
rm -f ./inbox/new/*

mdeliver inbox/ <./tmp.2
check 'message 2 is delivered verbatim via stdin' 'cmp ./tmp.2 ./inbox/new/*:2,'
rm -f ./inbox/new/*

mdeliver inbox/ <./tmp.3
check 'message 3 gets a newline via stdin' 'cmp ./tmp.3 ./inbox/new/*:2,'
rm -f ./inbox/new/*

mdeliver inbox/ <./tmp.4
check 'message 4 is delivered verbatim via stdin' 'cmp ./tmp.4 ./inbox/new/*:2,'
rm -f ./inbox/new/*


cat <<EOF >tmp.mbox
From nobody Thu Jan  1 00:59:59 1970
Subject: message 1

This is message 1.
From nobody Thu Jan  1 00:59:59 1970
Subject: message 2

This is message 2.

EOF

mdeliver -M inbox/ <./tmp.mbox
check 'mdeliver -M is tolerant with missing empty lines' 'ls inbox/new | wc -l | grep 2'

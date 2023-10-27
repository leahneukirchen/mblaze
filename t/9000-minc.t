#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 2

rm -rf test.dir
mkdir test.dir
(
cd test.dir

mkdir -p inbox/cur inbox/new
while read f; do touch "$f"; done <<!
inbox/new/1:2,
inbox/new/2
!

check_test 'minc' -eq 2 'minc inbox | wc -l'

while read f; do touch "$f"; done <<!
inbox/new/3:2,
inbox/new/4
!

check_test 'minc stdin' -eq 2 'echo inbox | minc | wc -l'
)

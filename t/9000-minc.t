#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 1

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

)

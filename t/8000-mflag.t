#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 12

rm -rf test.dir
mkdir test.dir
(
cd test.dir

cat <<! >seq
inbox/cur/1:2,
inbox/cur/2:2,
!

mkdir -p inbox/cur
while read f; do touch "$f"; done <seq
ln -sf inbox/cur/1:2, cur

export MAILSEQ=seq MAILCUR=cur

check 'mark seen' 'mflag -S 1 && [ -e "inbox/cur/1:2,S" ]'
check 'fix seq' 'mseq -f | mseq -S | wc -l | grep -qx 2'
check 'mark replied' 'mflag -R 1 && [ -e "inbox/cur/1:2,RS" ]'
check 'fix seq' 'mseq -f | mseq -S | wc -l | grep -qx 2'
check 'unmark replied' 'mflag -r 1 && [ -e "inbox/cur/1:2,S" ]'
check 'fix seq' 'mseq -f | mseq -S | wc -l | grep -qx 2'
check 'mark flagged' 'mflag -F 1 && [ -e "inbox/cur/1:2,FS" ]'
check 'fix seq' 'mseq -f | mseq -S | wc -l | grep -qx 2'
check 'unmark flagged' 'mflag -f 1 && [ -e "inbox/cur/1:2,S" ]'
check 'fix seq' 'mseq -f | mseq -S | wc -l | grep -qx 2'
check 'unmark seen' 'mflag -s 1 && [ -e "inbox/cur/1:2," ]'
check 'fix seq' 'mseq -f | mseq -S | wc -l | grep -qx 2'

)

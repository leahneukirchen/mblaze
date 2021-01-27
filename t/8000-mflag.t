#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 16

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
check_test 'fix seq' -eq 2 'mseq -f : | mseq -S | wc -l'
check 'mark replied' 'mflag -R 1 && [ -e "inbox/cur/1:2,RS" ]'
check_test 'fix seq' -eq 2 'mseq -f : | mseq -S | wc -l'
check 'unmark replied' 'mflag -r 1 && [ -e "inbox/cur/1:2,S" ]'
check_test 'fix seq' -eq 2 'mseq -f : | mseq -S | wc -l'
check 'mark flagged' 'mflag -F 1 && [ -e "inbox/cur/1:2,FS" ]'
check_test 'fix seq' -eq 2 'mseq -f : | mseq -S | wc -l'
check 'unmark flagged' 'mflag -f 1 && [ -e "inbox/cur/1:2,S" ]'
check_test 'fix seq' -eq 2 'mseq -f : | mseq -S | wc -l'
check 'unmark seen' 'mflag -s 1 && [ -e "inbox/cur/1:2," ]'
check_test 'fix seq' -eq 2 'mseq -f : | mseq -S | wc -l'
check 'mark trashed' 'mflag -T 1 && [ -e "inbox/cur/1:2,T" ]'
check_test 'fix seq' -eq 2 'mseq -f : | mseq -S | wc -l'
check 'unmark trashed' 'mflag -t 1 && [ -e "inbox/cur/1:2," ]'
check_test 'fix seq' -eq 2 'mseq -f : | mseq -S | wc -l'

)

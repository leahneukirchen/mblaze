#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 10

rm -rf test.dir
mkdir test.dir
(
cd test.dir

cat <<! >seq
inbox/cur/1:2,
inbox/cur/2:2,
inbox/cur/3:2,
inbox/cur/4:2,
inbox/cur/5_1:2,
 inbox/cur/6_2:2,
  inbox/cur/7_3:2,
   inbox/cur/8_4:2,
inbox/cur/9:2,
inbox/cur/10:2,
!

export MAILCUR=cur MAILSEQ=seq

check 'set current' 'mseq -C 1 && mseq . | grep "inbox/cur/1:2,"'
check 'set next' 'mseq -C + && mseq . | grep "inbox/cur/2:2,"'
check 'set prev' 'mseq -C .- && mseq . | grep "inbox/cur/1:2,"'
check 'last' 'mseq "$" | grep "inbox/cur/10:2,"'
check_test 'whole thread' -eq 4 'mseq 6= | wc -l'
check_test  'subthread' -eq 2 'mseq 7_ | wc -l'
check 'parent' 'mseq 6^ | grep "inbox/cur/5_1:2,"'
check_test 'range' -eq 3 'mseq 1:3 | wc -l'

cat <<! >seq
inbox/cur/1:2,
 inbox/cur/2:2,
  inbox/cur/3:2,
!

check_test  'whole thread at the end' -eq 3 'mseq 2= | wc -l'
check_test  'subthread at the end' -eq 2 'mseq 2_ | wc -l'

)

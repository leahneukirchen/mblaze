#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 6

rm -rf test.dir
mkdir test.dir
(
cd test.dir

mkdir -p "inbox/cur"

cat <<! | mmime >"inbox/cur/1:2,"
From: Rajwinder Kaur <rajwinder@example.com>
Subject: namaste
Date: Thu, 30 Mar 2017 15:42:05 +0200
Message-Id: <EOH1F3NUOY.2KBVMHSBFATNY@example.org>

!

cat <<! | mmime >"inbox/cur/2:2,"
From: имярек <имярек@example.com>
Subject: Здравствуйте
Date: Thu, 30 Mar 2017 15:42:10 +0200
Message-Id: <EOH8EKEA1Z.2YAIFN5KVCO6Z@example.org>

!

cat <<! | mmime >"inbox/cur/3:2,"
From: Pera Perić <pera.p@example.com>
Subject: Здраво
Date: Thu, 30 Mar 2017 15:40:32 +0200
Message-Id: <EOH8EEXVXF.32ZMPPCNGCJH5@example.org>

!

cat <<! | mmime >"inbox/cur/4:2,"
From: Perico de los palotes <palotes@example.com>
Subject: Hola
Date: Thu, 30 Mar 2017 16:20:11 +0200
Message-Id: <EOH8F4IA07.2FU96ZKHJEVFC@example.org>

!

cat <<! >seq
inbox/cur/1:2,
inbox/cur/2:2,
inbox/cur/3:2,
inbox/cur/4:2,
!

export MAILSEQ=seq

check_same 'filename' 'msort -F :' 'cat seq'

cat <<! >expect
inbox/cur/3:2,
inbox/cur/1:2,
inbox/cur/2:2,
inbox/cur/4:2,
!
check_same 'date' 'msort -d :' 'cat expect'

cat <<! >expect
inbox/cur/4:2,
inbox/cur/2:2,
inbox/cur/1:2,
inbox/cur/3:2,
!
check_same 'reverse date' 'msort -dr :' 'cat expect'

cat <<! >expect
inbox/cur/2:2,
inbox/cur/3:2,
inbox/cur/4:2,
inbox/cur/1:2,
!
check_same 'from' 'msort -f :' 'cat expect'

cat <<! >expect
inbox/cur/3:2,
inbox/cur/2:2,
inbox/cur/4:2,
inbox/cur/1:2,
!
check_same 'subject' 'msort -s :' 'cat expect'

check 'non-existent mail' 'msort -s /does/not/exist ; [ $? -eq 1 ]'

)

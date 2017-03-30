#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 2

rm -rf test.dir
mkdir test.dir
(
cd test.dir

mkdir -p "inbox/cur"

cat <<! | mmime >"inbox/cur/1:2,"
From: Rajwinder Kaur <rajwinder@example.com>
Subject: Hello
Date: Thu, 30 Mar 2017 15:42:05 +0200
Message-Id: <EOH1F3NUOY.2KBVMHSBFATNY@example.org>

body
!

cat <<! >seq
inbox/cur/1:2,
!

export MAILSEQ=seq

check_same 'ISO date' 'TZ=utc mscan -f "%16D" 1' 'echo "2017-03-30 13:42"'
check_same 'from name' 'mscan -f "%f" 1' 'echo "Rajwinder Kaur"'

)

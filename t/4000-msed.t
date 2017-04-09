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
Subject: Hello
Date: Thu, 30 Mar 2017 15:42:05 +0200
Message-Id: <EOH1F3NUOY.2KBVMHSBFATNY@example.org>

body
!

cat <<! >seq
inbox/cur/1:2,
!

export MAILSEQ=seq

check 'append new' 'msed "/foobar/a/value/" 1 | grep "Foobar: value"'
check 'append existing' 'msed "/subject/a/world/" 1 | grep -v "world"'
check_test 'append multiple' -eq 2 'msed "/foo/a/catch/;/bar/a/catch/" 1 | grep -c catch'
check 'change' 'msed "/subject/c/world/" 1 | grep "Subject: world"'
check 'delete' 'msed "/message-id/d" 1 | grep -v "Message-Id"'
check 'substitute' 'msed "/subject/s/\(Hello\)/\1 World/" 1 | grep "^Subject: Hello World$"'

)

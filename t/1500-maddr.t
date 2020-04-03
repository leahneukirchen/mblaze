#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 11

rm -rf test.dir
mkdir test.dir
(
cd test.dir

mkdir -p "inbox/cur"

cat <<! | mmime >"inbox/cur/1:2,"
From: Rajwinder Kaur <rajwinder@example.com>
Obs-Test: Rajwinder Kaur <@example.org:rajwinder@example.com>
Subject: namaste
Date: Thu, 30 Mar 2017 15:42:05 +0200
Message-Id: <EOH1F3NUOY.2KBVMHSBFATNY@example.org>

!

cat <<! | mmime >"inbox/cur/2:2,"
From: имярек <имярек@example.com>, Rajwinder Kaur <rajwinder@example.com>
Subject: Здравствуйте
Date: Thu, 30 Mar 2017 15:42:10 +0200
Message-Id: <EOH8EKEA1Z.2YAIFN5KVCO6Z@example.org>

!

cat <<! | mmime >"inbox/cur/3:2,"
From: rajwinder@example.com
Subject: Здраво
Date: Thu, 30 Mar 2017 15:40:32 +0200
Message-Id: <EOH8EEXVXF.32ZMPPCNGCJH5@example.org>

!

cat <<! | mmime >"inbox/cur/4:2,"
From: Perico de los palotes
Subject: Hola
Date: Thu, 30 Mar 2017 16:20:11 +0200
Message-Id: <EOH8F4IA07.2FU96ZKHJEVFC@example.org>
Foo: Perico de los palotes <palotes@example.com>
Long: heeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeelloooooooooooooooooo@woooooooooooooooooooooooooooooooooooooorld.com

!

# from rfc2047.c
cat <<! >"inbox/cur/5:2,"
DecodeISO8859: =?ISO-8859-1?Q?Keld_J=F8rn_Simonsen?= <keld@dkuug.dk>
DecodeLongISO8859: =?ISO-8859-1?B?SWYgeW91IGNhbiByZWFkIHRoaXMgeW8=?= 
 =?ISO-8859-2?B?dSB1bmRlcnN0YW5kIHRoZSBleGFtcGxlLg==?= z 
 =?ISO-8859-1?Q?a?= =?ISO-8859-2?Q?_b?= <foo@example.com>
DecodeUTF8: =?UTF-8?Q?z=E2=80?= =?UTF-8?Q?=99z?= <bar@example.com>

!

cat <<! >seq
inbox/cur/1:2,
inbox/cur/2:2,
inbox/cur/3:2,
inbox/cur/4:2,
inbox/cur/5:2,
!

export MAILSEQ=seq

check_same 'from one' 'maddr 1' 'echo "Rajwinder Kaur <rajwinder@example.com>"'
check_same 'from address' 'maddr -a 1' 'echo "rajwinder@example.com"'
check_same 'from one' 'maddr -h obs-test 1' 'echo "Rajwinder Kaur <rajwinder@example.com>"'

cat <<! >expect
имярек <имярек@example.com>
Rajwinder Kaur <rajwinder@example.com>
!
check_same 'from two' 'maddr 2' 'cat expect'

check_same 'from addr only' 'maddr 3' 'echo "rajwinder@example.com"'
check_test 'from name only' -eq 0 'maddr 4 | wc -l'
check_same 'specific header' 'maddr -h foo 4' 'echo "Perico de los palotes <palotes@example.com>"'
check_same 'long addr' 'maddr -h long 4' 'echo "heeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeelloooooooooooooooooo@woooooooooooooooooooooooooooooooooooooorld.com"'
check_same 'decode iso8859' 'maddr -h DecodeISO8859 5' 'echo "Keld Jørn Simonsen <keld@dkuug.dk>"'
check_same 'decode long iso8859' 'maddr -h DecodeLongISO8859 5' 'echo "\"If you can read this you understand the example. z a b\" <foo@example.com>"'
check_same 'decode utf8' 'maddr -h DecodeUTF8 5' 'echo "z’z <bar@example.com>"'

)

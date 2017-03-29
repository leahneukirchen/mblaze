#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 11

rm -rf test.dir
mkdir test.dir
(
cd test.dir

mkdir -p inbox/cur
touch "inbox/cur/1:2,S"
touch "inbox/cur/2:2,ST"
touch "inbox/cur/3:2,SRT"
touch "inbox/cur/4:2,SRFT"
touch "inbox/cur/5:2,T"
touch "inbox/cur/6:2,SRF"
touch "inbox/cur/7:2,SR"
touch "inbox/cur/8:2,SF"
touch "inbox/cur/9:2,"

check_same 'flag trashed' 'mlist inbox | mpick :T' 'mlist -T inbox'
check_same 'flag not trashed' 'mlist inbox | mpick -t "!trashed"' 'mlist -t inbox'
check_same 'flag seen' 'mlist inbox | mpick :S' 'mlist -S inbox'
check_same 'flag not seen' 'mlist inbox | mpick -t !seen' 'mlist -s inbox'
check_same 'flag seen and trashed' 'mlist inbox | mpick :S :T' 'mlist -ST inbox'
check_same 'flag seen and not trashed' 'mlist inbox | mpick -t "seen && !trashed"' 'mlist -St inbox'
check_same 'flag replied' 'mlist inbox | mpick :R' 'mlist -R inbox'
check_same 'flag forwarded' 'mlist inbox | mpick :F' 'mlist -F inbox'


cat <<! | mmime >"inbox/cur/1:2,S"
From: Peter Example <peter@example.org>
Subject: Hey whats up?
Date: Thu, 30 Mar 2017 15:41:17 +0200
Message-Id: <EOH1EP40II.28EN1IMPMSUVS@example.org>

Greetings
!

cat <<! | mmime >"inbox/cur/9:2,"
From: Peter Example <peter@example.org>
Subject: wow nice subject
Date: Thu, 30 Mar 2017 15:42:00 +0200
Message-Id: <EOH1EWLI6M.3AIXSIEV1VFBQ@example.org>

shit happens
!

cat <<! | mmime >"inbox/cur/5:2,T"
From: Obvious spam <script@kiddy.com>
Subject: look at this awesome pdf
Date: Thu, 30 Mar 2017 15:42:05 +0200
Message-Id: <EOH1F3NUOY.2KBVMHSBFATNY@example.org>

Check my resume!

Greetings

#application/pdf ../../mshow
!

check 'search subject' 'mlist inbox | mpick /wow | grep -q inbox/cur/9:2,'
check 'search addr' 'mlist inbox | mpick peter@example.org | wc -l | grep -qx 2'
check 'search name' 'mlist inbox | mpick "Peter Example" | wc -l | grep -qx 2'
check 'search spam' 'mlist inbox | mpick -t "trashed && subject =~ \"pdf\"" | wc -l | grep -qx 1'

)

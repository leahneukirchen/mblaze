#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 9

rm -rf test.dir
mkdir test.dir
(
cd test.dir

mkdir -p "inbox/cur"

cat <<! | mmime >"inbox/cur/1:2,"
From: Piet Pompies <piet@lpompies.com>
Subject: wow nice subject
Date: Thu, 30 Mar 2017 15:42:05 +0200
Message-Id: <EOH1F3NUOY.2KBVMHSBFATNY@example.org>

shit happens
!

cat <<! | mmime >"inbox/cur/2:2,"
From: Piet Pompies <piet@pompies.com>
Subject: Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.
Date: Thu, 30 Mar 2017 15:42:05 +0200
Message-Id: <EOH1F3NUOY.2KBVMHSBFATNY@example.org>

Greetings
!

cat <<! | mmime >"inbox/cur/3:2,"
From: Piet Pompies <piet@pompies.com>
Subject: 1 multi subject one
Subject: 2 multi subject two
Subject: 3 multi subject three
Date: Thu, 30 Mar 2017 15:42:05 +0200
Message-Id: <EOH1F3NUOY.2KBVMHSBFATNY@example.org>

!

cat <<! >seq
inbox/cur/1:2,
inbox/cur/2:2,
inbox/cur/3:2,
!

export MAILSEQ=seq

check 'subject' 'magrep subject:nice | wc -l | grep -qx 1'
check 'ignorecase' 'magrep -i subject:NICE | wc -l | grep -qx 1'
check 'invert' 'magrep -v subject:nice | wc -l | grep -qx 2'
check 'max matches' 'magrep -m 2 from:Piet | wc -l | grep -qx 2'
check 'long subject' 'magrep subject:aliqua | wc -l | grep -qx 1'

echo 'inbox/cur/1:2,: subject: wow nice subject' >expect
check_same 'print' 'magrep -p subject:nice' 'cat expect'

echo 'inbox/cur/1:2,: subject: nice' >expect
check_same 'print match' 'magrep -po subject:nice' 'cat expect'

echo 'nice' >expect
check_same 'print match only' 'magrep -o subject:nice' 'cat expect'

echo 'inbox/cur/3:2,' >expect
check_same 'multiple subjects' 'magrep subject:multi' 'cat expect'

)

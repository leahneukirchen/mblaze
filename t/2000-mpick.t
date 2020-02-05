#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 28

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
Foo: bar

Check my resume!

Greetings

#application/pdf ../../mshow
!

cat <<! >shebang
#!$(command -v mpick)
from.addr == "peter@example.org" && from.disp == "Peter Example"
!
chmod +x shebang

check 'search subject' 'mlist inbox | mpick /wow | grep -q inbox/cur/9:2,'
check_test 'search addr' -eq 2 'mlist inbox | mpick peter@example.org | wc -l'
check_test 'search name' -eq 2 'mlist inbox | mpick "Peter Example" | wc -l'
check_test 'search spam' -eq 1 'mlist inbox | mpick -t "trashed && subject =~ \"pdf\"" | wc -l'
check_test 'any header' -eq 1 'mlist inbox | mpick -t "\"Foo\" =~~ \"bar\"" | wc -l'
check_test 'addr decode addr' -eq 2 'mlist inbox | mpick -t "from.addr == \"peter@example.org\"" | wc -l'
check_test 'addr decode disp' -eq 2 'mlist inbox | mpick -t "from.disp == \"Peter Example\"" | wc -l'
check_test 'shebang' -eq 2 'mlist inbox | ./shebang | wc -l'
check_test 'ternary' -eq 2 'mlist inbox | mpick -t "from.addr == \"peter@example.org\" ? print : skip" | wc -l'

check_same 'pipe command' 'mlist inbox | mpick -t "print |\"cat -n\" && skip"' 'mlist inbox | cat -n'
check_same 'create file' 'mlist inbox | mpick -t "print >\"foo\" && skip" && cat foo' 'mlist inbox'
check_same 'overwrite file' 'mlist inbox | mpick -t "print >\"foo\" && skip" && cat foo' 'mlist inbox'
check_same 'append file' 'mlist inbox | mpick -t "print >>\"foo\" && skip" && cat foo' 'mlist inbox && mlist inbox'

check_same 'unknown ident' 'mlist inbox | mpick -t "let x = x in x" 2>&1' \
	"echo \"mpick: parse error: argv:1:9: unknown expression at 'x in x'\""

cat <<! >expr
let foo = from.addr == "peter@example.org"
let bar = from.disp == "Peter Example"
# random comment
in
  foo && bar # another comment
!
check_test 'let expression' -eq 2 'mlist inbox | mpick ./expr | wc -l'

cat <<! >expr
let foo = from.addr == "peter@example.org"
let bar = from.disp == "Peter Example"
# random comment
in
  foo && foo
!
check_test 'let expression double free' -eq 2 'mlist inbox | mpick ./expr | wc -l'

cat <<! >expr
let foo =
  let bar = from.disp == "Peter Example"
  in
    bar && from.addr == "peter@example.org"
in
  foo
!
check_test 'let expression nested' -eq 2 'mlist inbox | mpick ./expr | wc -l'

cat <<! >expr
let foo = from.addr == "peter@example.org"
let bar = foo && subject =~ "wow"
in
  bar
!
check_test 'let scoping' -eq 1 'mlist inbox | mpick ./expr | wc -l'

cat <<! >expr
let foo = from.addr == "peter@example.org"
let bar = from.disp == "Peter Example"
in
  foo |"sed ""s/^/1:&/""" && bar |"sed ""s/^/2:&/""" && skip
!
check_test 'multi redir' -eq 4 'mlist inbox | mpick ./expr | wc -l'
check_test 'multi redir prefixes' -eq 2 'mlist inbox | mpick ./expr | cut -d: -f1 | sort -u | wc -l'

)

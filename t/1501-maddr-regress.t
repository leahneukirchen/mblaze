#!/bin/sh
cd ${0%/*}
. ./lib.sh
plan 27

check_addr() {
  printf "From: %s\n" "$1" | check_test "parse $1" = "$2" "maddr /dev/stdin"
}

check_addr 'foo@example.org' 'foo@example.org'
check_addr '<foo@example.org>' 'foo@example.org'
check_addr 'bar <foo@example.org>' 'bar <foo@example.org>'
check_addr 'bar quux <foo@example.org>' 'bar quux <foo@example.org>'
check_addr 'bar  quux <foo@example.org>' 'bar quux <foo@example.org>'
check_addr '<foo@example.org> (bar)' 'bar <foo@example.org>'
check_addr '"Real Name" <foo@example.org>' 'Real Name <foo@example.org>'
check_addr '"Dr. Real Name" <foo@example.org>' 'Dr. Real Name <foo@example.org>'
check_addr '"Real Name" (show this) <foo@example.org>' 'Real Name (show this) <foo@example.org>'
check_addr '"Real Name" <foo@example.org> (ignore this)' 'Real Name (ignore this) <foo@example.org>'

check_addr '(nested (comments mail@here ) heh) "yep (yap)" <foo@example.org>' 'yep (yap) <foo@example.org>'

check_addr 'ignore-this: <foo@example.org>' 'foo@example.org'
check_addr 'ignore-this : <foo@example.org>' 'foo@example.org'
check_addr '"foo"@example.org' 'foo@example.org'
check_addr '"Barqux" "foo"@example.org' 'Barqux <foo@example.org>'
check_addr 'Space Man <"space man"@example.org>' 'Space Man <"space man"@example.org>'
check_addr 'Space Man <"space  man"@example.org>' 'Space Man <"space  man"@example.org>'
check_addr '<spaceman@example.org  >' 'spaceman@example.org'
check_addr '< spaceman@example.org  >' 'spaceman@example.org'
check_addr '<spaceman@(wtf)example.org  >' 'spaceman@example.org'
check_addr 'space\man@example.org' 'spaceman@example.org'

check_addr 'what is <"this<evil(h\"eh)sh>it"@example.org>' 'what is <"this<evil(h\"eh)sh>it"@example.org>'

check_addr 'foo@example.org <foo@example.org>' 'foo@example.org'

check_addr 'foo@[::1] (ipv6)' 'ipv6 <foo@[::1]>'

# invalid addresses
check_addr '<Foo Bar <foobar@qux.com>' 'foobar@qux.com'
check_addr '"abc@def"@ghi' ''
check_addr '"foo@" <bar.com foo@bar.com>' '"foo@" <bar.comfoo@bar.com>'

#!/bin/sh -e
cd ${0%/*}
. ./lib.sh

plan 9

cat <<EOF >tmp
Header: foo
Header2: bar
Header-Three: quux
Header_Four: ding

Body
EOF

check_same 'Header' 'mhdr -h Header ./tmp' 'echo foo'
check_same 'Header2' 'mhdr -h Header2 ./tmp' 'echo bar'
check_same 'Header-Three' 'mhdr -h Header-Three ./tmp' 'echo quux'
check_same 'Header_Four' 'mhdr -h Header_Four ./tmp' 'echo ding'

check_same 'header' 'mhdr -h header ./tmp' 'echo foo'
check_same 'header2' 'mhdr -h header2 ./tmp' 'echo bar'
check_same 'header-Three' 'mhdr -h header-Three ./tmp' 'echo quux'
check_same 'header_Four' 'mhdr -h header_Four ./tmp' 'echo ding'

check 'issue 235' 'mhdr ./tmp |grep -i header_four'

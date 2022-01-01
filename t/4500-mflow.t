#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 12

rm -rf test.dir
mkdir test.dir
(
cd test.dir

export PIPE_CONTENTTYPE='text/plain; format=flowed'
export COLUMNS=80

cat <<! >a
this 
is 
flowed.
!
cat <<! >b
this is flowed.
!
check 'simple reflow' 'mflow <a | cmp - b'

cat <<! >a
this 
is  
two spaces.
!
cat <<! >b
this is  two spaces.
!
check 'simple space stuffing' 'mflow <a | cmp - b'

cat <<! >a
this 
is 
flowed.
this is fixed.
!
cat <<! >b
this is flowed.
this is fixed.
!
check 'simple fixed' 'mflow <a | cmp - b'

cat <<! >a
> this 
> is 
> quoted.
this 
is 
unquoted.
!
cat <<! >b
> this is quoted.
this is unquoted.
!
check 'simple quoted' 'mflow <a | cmp - b'

(
export PIPE_CONTENTTYPE='text/plain; format=flowed; delsp=yes'

cat <<! >a
> this 
> is 
> delsp.

> double  
> spaced
!
cat <<! >b
> thisisdelsp.

> double spaced
!
check 'simple delsp' 'mflow <a | cmp - b'
)

cat <<! >a
this 
is 
way more than eighty chars which is the terminal width to flow. 
this 
is 
way more than eighty chars which is the terminal width to flow. 
!
cat <<! >b
this is way more than eighty chars which is the terminal width to flow. this is
way more than eighty chars which is the terminal width to flow. 
!
check 'simple wrap' 'mflow <a | cmp - b'

cat <<! >a
this 
is 
way more than eighty chars which is the terminal width to flow. 
averylongwordcomeshere. 
this 
is 
way more than eighty chars which is the terminal width to flow.
!
cat <<! >b
this is way more than eighty chars which is the terminal width to flow. 
averylongwordcomeshere. this is way more than eighty chars which is the
terminal width to flow.
!
check 'more complex wrap' 'mflow <a | cmp - b'

cat <<! >a
foo 
bar. 

quux.
!
cat <<! >b
foo bar. 

quux.
!
check 'space before empty line' 'mflow <a | cmp - b'

cat <<! >a
Aaaaa bbbbb ccccc ddddd eeeee aaaaa bbbbb ccccc ddddd eeeee 
aaaaa bbbbb ccccc ddddd eeeee aaaaa bbbbb ccccc ddddd eeeee 
aaaaa bbbbb ccccc 
ffffff gggggg hhhhhh iiiiii.
!
cat <<! >b
Aaaaa bbbbb ccccc ddddd eeeee aaaaa bbbbb ccccc ddddd eeeee aaaaa bbbbb ccccc
ddddd eeeee aaaaa bbbbb ccccc ddddd eeeee aaaaa bbbbb ccccc ffffff gggggg
hhhhhh iiiiii.
!
check 'fixed lines are wrapped too' 'mflow <a | cmp - b'

cat <<! >a
some 
wrapped. 
-- 
signature
!
cat <<! >b
some wrapped. 
-- 
signature
!
check 'passes usenet signature marker as is' 'mflow <a | cmp - b'

cat <<! >a
some regular text being force wrapped because the line is way too long oh no who writes so long lines.
!
cat <<! >b
some regular text being force wrapped because the line is way too long oh no
who writes so long lines.
!
check 'force wrapping' 'mflow -f <a | cmp - b'

cat <<! >a
> some regular text being force wrapped because the line is way too long oh no who writes so long lines.
!
cat <<! >b
> some regular text being force wrapped because the line is way too long oh no
> who writes so long lines.
!
check 'force wrapping of quoted text' 'mflow -f <a | cmp - b'

)

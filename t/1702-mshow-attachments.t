#!/bin/sh -e
cd ${0%/*}
. ./lib.sh
plan 2

# Different naming scenarios for named parts
cat <<EOF >tmp
Content-Type: multipart/mixed; boundary=----_NextPart_000_00DE_01D6A2E8.A7446C80

------_NextPart_000_00DE_01D6A2E8.A7446C80

no header, part is not attachment
------_NextPart_000_00DE_01D6A2E8.A7446C80
Content-Type: text/plain; charset=UTF-8

CT w/o name, part is not attachment
------_NextPart_000_00DE_01D6A2E8.A7446C80
Content-Type: text/plain; charset=UTF-8; name="ctn.txt"

CT with name, part is attachment
------_NextPart_000_00DE_01D6A2E8.A7446C80
Content-Disposition: attachment

CD w/o filename, part is not attachment
------_NextPart_000_00DE_01D6A2E8.A7446C80
Content-Type: text/plain; charset=UTF-8
Content-Disposition: attachment

CD w/o filename, CT w/o name, part is not attachment
------_NextPart_000_00DE_01D6A2E8.A7446C80
Content-Type: text/plain; charset=UTF-8; name="cd_ctn.txt"
Content-Disposition: attachment

CD w/o filename, CT with name, part is attachment
------_NextPart_000_00DE_01D6A2E8.A7446C80
Content-Disposition: attachment; filename="cdf.txt"

CD with filename, part is attachment
------_NextPart_000_00DE_01D6A2E8.A7446C80
Content-Type: text/plain; charset=UTF-8; name="cdf_ct.txt"
Content-Disposition: attachment

CD with filename, CT w/o name, part is attachment
------_NextPart_000_00DE_01D6A2E8.A7446C80
Content-Type: text/plain; charset=UTF-8; name="cdf_ctn.txt"
Content-Disposition: attachment; filename="cdf_ctn.txt"

CD with filename, CT with name, part is attachment
------_NextPart_000_00DE_01D6A2E8.A7446C80--
EOF

check 'mail has 10 parts' 'mshow -t ./tmp | wc -l | grep 11'
check 'mail has 5 named parts/attachments' 'mshow -t ./tmp | grep .*name=\".*\" | wc -l | grep 5'

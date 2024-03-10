#!/bin/sh -e
cd ${0%/*}
. ./lib.sh

plan 21

cat <<EOF >tmp
References: <aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa@a> <bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb@b> <ccccccccccccccccccccccccccccccc@c>

Body
EOF

# https://github.com/leahneukirchen/mblaze/issues/20

check 'mime -r runs' 'mmime -r <tmp >tmp2'
check 'no overlong lines' 'awk "{if(length(\$0)>=80)exit 1}" <tmp2'
check 'no QP when unecessary' ! grep -qF "=?" tmp2
check 'no further mime necessary' 'mmime -c <tmp2'

cat <<EOF >tmp2
Subject: inclusion test

#message/rfc822 $PWD/tmp
EOF

check 'include works' 'mmime <tmp2 | grep Body'
check 'include sets filename' 'mmime <tmp2 | grep filename=tmp'


cat <<EOF >tmp2
Subject: inclusion test no filename

#message/rfc822 $PWD/tmp>
EOF

check 'include works, overriding filename' 'mmime <tmp2 | grep Disposition | grep -v filename=tmp'


cat <<EOF >tmp2
Subject: inclusion test with other disposition

#message/rfc822#inline $PWD/tmp>
EOF

check 'include works, overriding filename' 'mmime <tmp2 | grep Disposition | grep inline'


cat <<EOF >tmp2
Subject: message with content-type
Content-Type: text/plain; format=flowed

This message has format-flowed.
EOF

check 'content-type is respected if found in input' 'mmime -r <tmp2 |grep format=flowed'

cat <<EOF >tmp2
Subject: message with content-transfer-encoding
Content-Transfer-Encoding: quoted-printable

This message has already encoded. f=C3=B6=C3=B6.
EOF


check 'content-transfer-encoding is respected if found in input' 'mmime -r <tmp2 |grep f=C3=B6=C3=B6'

cat <<EOF >tmp2
Subject: message with content-type
Content-Type: text/plain; format=flowed

This message has format-flowed.

#message/rfc822 $PWD/tmp

This part too.
EOF


check 'content-type is respected if found in input, for multipart/mixed' 'mmime <tmp2 |grep format=flowed'

cat <<EOF >tmp2
Subject: message with content-transfer-encoding
Content-Transfer-Encoding: Quoted-Printable

This message has already encoded. f=C3=B6=C3=B6.

#message/rfc822 $PWD/tmp

This part too.
EOF

check 'content-transfer-encoding is respected if found in input, for multipart/mixed' 'mmime <tmp2 |grep f=C3=B6=C3=B6'

cat <<EOF >tmp2
From: Kerstin Krüger <krueger@example.com>

Body.
EOF

check 'non-ASCII is encoded as UTF-8' 'mmime <tmp2 | grep "UTF-8.*=C3=BC"'

cat <<EOF >tmp2
From: "Krüger, Kerstin" <krueger@example.com>

Body.
EOF

check 'non-ASCII quoted-strings are encoded as one encoded-word' 'mmime <tmp2 | grep "UTF-8.*=2C_"'

check 'non-ASCII quoted-strings are encoded without quotes' 'mmime <tmp2 | grep -v "=22"'

cat <<EOF >tmp2
From: "kerstin krueger"@example.com

Body.
EOF

check 'non-encoded quoted-strings are kept correctly' 'mmime <tmp2 | grep \"@'

cat <<EOF >tmp2
Subject: inclusion without further content

#message/rfc822#inline $PWD/tmp
EOF

check 'no empty parts are generated after inclusion lines' '! mmime <tmp2 | mshow -t - | grep -q size=0'

cat <<EOF >tmp2
Subject: Strict mode

Body with lines longer than 78 characters
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
EOF

check 'body lines longer than 78 characters needs MIME formatting' '! mmime -c <tmp2'
check 'MBLAZE_RELAXED_MIME allows body lines longer than 78 characters' 'MBLAZE_RELAXED_MIME= mmime -c <tmp2'

cat <<EOF >tmp2
DKIM-Signature: v=1; a=rsa-sha256; q=dns/txt; c=relaxed/relaxed; t=1710013705;
	s=s1; d=tuta.io;
	h=From:From:To:To:Subject:Subject:Content-Description:Content-ID:Content-Type:Content-Type:Content-Transfer-Encoding:Cc:Date:Date:In-Reply-To:MIME-Version:MIME-Version:Message-ID:Message-ID:Reply-To:References:Sender; bh=Jr8DQlZ7RwdJv94m7ZT/v+cv/WFsgjxpMRsHvnNfgGY=;
	b=NXRl0YxYtVsWrR8v7tVKnvsnCSrBqqaf2h3m8OVGlzG0OqMqGcWg7fVk6x4nTYV+
	+05afZrGfIwcfFwIe/LLvT0d3/12t4+cs/FQvmEcFUN+n2buQwt5sn8f76UUlvNMrGz
	Xbq8HAdwhA364yWABa7DrF1EGysC8bEDJcCtSs/Wz3TL2A/MEeItEF+VijtgWUwoOwn
	rFKkCg5Df+IOd4gEBS/KYLbzcMB1dvqy+ut2LA2+NZpzJQPgbJzWAYieT9KYgoS+hKS
	5FfknNT+hKZz18IBEWH1UWbI+CcLRR8Sr80x2DZUKq8ryC5RmV5/uAc5Up03b/KZGRU
	NsiBAQCx3w==
EOF

check 'header words longer then 78 characters do not cause empty lines (#257)' 'mmime < tmp2 | awk "NR < 5 && length == 0 { exit 1 }"'

check 'header words longer then 78 characters are printed on their own line' 'mmime < tmp2 |grep "^[ 	]*h=From.*Sender;$"'

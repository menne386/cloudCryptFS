#!/bin/bash
# Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

if [ ! -f "$PASSFILE" ]; then
	echo "Provide a password in the $PASSFILE file"
	exit 1
fi

PASS=`cat $PASSFILE`

if [ ! -d "/target/dta" ]; then
	echo "Creating vault:"
	/cloudCryptFS.docker -osrc=/target/ -opass="$PASS",keyfile="$KEYFILE" --create yes --log-level $LOGLEVEL || exit 1
fi



echo "Mounting vault:"
/cloudCryptFS.docker -odirect_io,use_ino --src /target/ -opass="$PASS",keyfile="$KEYFILE" /mnt || exit 1

echo "Backing up data:"

rsync -av /source/ /target/

echo "Closing vault:"
fusermount -u /mnt/
sleep 5

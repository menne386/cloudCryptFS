#!/bin/bash
# Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.



UIDGID="`id -u`:`id -g`"

SOURCES=""

TARGET=""
SECRETS=""

function usage {
	echo "Usage: $0 -s PATH NAME -s PATH NAME -s PATH NAME -t PATH -c PATH"
	exit
}

while (( "$#" )); do
	if [[ "$1" == "-s" ]]; then
		shift
		echo "Adding source $1 with name $2"
		SOURCES="$SOURCES -v $1:/source/$2"
		shift
	fi
	if [[ "$1" == "-t" ]]; then
		shift
		echo "Adding target $1"
		TARGET="$1"
	fi
	if [[ "$1" == "-c" ]]; then
		shift
		echo "Adding secrets $1"
		SECRETS="$1"
	fi
	shift
done

if [ "$SOURCES" == "" ];then
	usage
fi

if [ "$TARGET" == "" ];then
	usage
fi

if [ "$SECRETS" == "" ];then
	usage
fi

#echo $SOURCES

docker run --name "cloudcryptfs_backup" --rm -v "$SECRETS:/secrets:z" -v "$TARGET:/target:z" $SOURCES --privileged --security-opt apparmor:unconfined --cap-add SYS_ADMIN --device /dev/fuse -it menne386/cloudcryptfs_backup --uidgid $UIDGID 


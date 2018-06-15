#!/bin/bash
# Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
rm -rf dta ._lock config.json log.txt
echo "create:"
./cloudCryptFS --create yes --pass menne
echo "Mount"
./cloudCryptFS --pass menne mntpoint --loglevel 10 
echo "Copy file to FS"
touch mntpoint/._stats
cp /usr/bin/gcc mntpoint/gcc
cp ../testfile mntpoint/testfile
echo `md5sum mntpoint/*`
cat mntpoint/._stats
ls -lha mntpoint
echo "unmount"
fusermount -u mntpoint
echo "migrate"
./cloudCryptFS --pass menne --migrateto 1.1

echo "Mount"
./cloudCryptFS --pass menne mntpoint
sleep 2
cat mntpoint/._stats
echo `md5sum mntpoint/*`
echo "unmount"
fusermount -u mntpoint
echo `md5sum /usr/bin/gcc ../testfile`

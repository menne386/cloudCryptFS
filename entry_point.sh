#!/bin/bash
# Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

echo "FS:"
/cloudCryptFS.docker -odirect_io,use_ino --src /srv/ --pass menne /mnt

echo "log file = /srv/log.txt" > /etc/rsyncd.conf
echo "[files]" >> /etc/rsyncd.conf
echo "path = /mnt" >> /etc/rsyncd.conf
echo "comment = Backups" >> /etc/rsyncd.conf
echo "read only = false" >> /etc/rsyncd.conf
echo "timeout = 300" >> /etc/rsyncd.conf
echo "uid = 0" >> /etc/rsyncd.conf
echo "gid = 0" >> /etc/rsyncd.conf

echo "RSYNC:"
rsync --daemon
echo "READY"



tail -f /srv/log.txt 
read -p "Press enter to continue"

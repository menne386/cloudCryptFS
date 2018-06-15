#!/bin/bash
# Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
rm -rf dta
rm log.txt
make -j5 &&
./cloudCryptFS -oattr_timeout=0 -opass=menne -o allow_other test &&
cd test &&
mkfifo testfifo
ls -lha
cd ..
fusermount -u test



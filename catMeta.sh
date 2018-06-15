#!/bin/bash
# Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
./cloudCryptFS -opass=menne test
cat test/._meta
sleep 1
fusermount -u test

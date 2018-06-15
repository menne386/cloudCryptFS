#!/bin/bash
# Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

for VARIABLE in 1 2 3 4 5 6 7 8 9
do
./fstest_docker.sh $@ 
cp "test/log.txt" "test/run$VARIABLE.txt"
done
grep "Result: " test/run*

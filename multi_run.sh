#!/bin/bash
# Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

NUM=10

if [[ "$1" == "-n" ]]; then
	shift
	NUM=$1
	shift
fi



for (( VARIABLE=1; VARIABLE<=NUM; VARIABLE++ ))
do
./fstest_docker.sh $@ 
cp "test/log.txt" "test/run$VARIABLE.txt"
done
grep "Result: " test/run*

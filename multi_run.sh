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

rm -rf test/run*

echo "Running $NUM tests with parameters: $@"

for (( VARIABLE=1; VARIABLE<=NUM; VARIABLE++ ))
do
./fstest_docker.sh -n $VARIABLE $@ 
done

echo "$NUM tests completed with parameters: $@"

source list_errors.sh

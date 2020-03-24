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

echo "Passed:"
grep "PASS" test/run*/log.txt
echo "Warnings:"
grep "WARNING" test/run*/log.txt
echo "Errors:"
grep "FAIL" test/run*/log.txt
grep "ERROR" test/run*/log.txt
echo "Coredumps:"
ls test/run*/core* -lha 2>/dev/null

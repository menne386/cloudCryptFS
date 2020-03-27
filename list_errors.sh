#!/bin/bash
# Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

echo "Passed:"
grep "PASS" test/run*/log.txt
echo "Warnings & Errors & Fails:"
grep "WARNING\|ERROR\|FAIL" test/run*/log.txt
echo "Coredumps:"
ls test/run*/core* -lha 2>/dev/null

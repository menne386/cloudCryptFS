#!/bin/bash
# Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

echo "Passed:"
grep -Hc "PASS" test/run*/log.txt
echo "Warnings & Errors & Fails:"
grep -H "WARNING\|ERROR\|FAIL" test/run*/log.txt | sort -u
echo "Coredumps:"
ls test/run*/core* -lha 2>/dev/null

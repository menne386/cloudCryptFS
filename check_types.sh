#!/bin/bash
# Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
grep  -E "(off_t|uid_t|gid_t|size_t|mode_t|dev_t)" -rw src/* 


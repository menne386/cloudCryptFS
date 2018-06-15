#!/bin/bash
# Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
function version_gt() { test "$(printf '%s\n' "$@" | sort -V | head -n 1)" != "$1"; }

if version_gt `uname -r` "4.2.0"; then
		docker run --name cloudcryptfs_fstest --rm -v "$(pwd)/test:/output:z" --privileged --security-opt apparmor:unconfined --cap-add SYS_ADMIN --device /dev/fuse -it menne386/cloudcryptfs_fstest $@
else 
		echo Using pid=host workaround.
		docker run --name cloudcryptfs_fstest --rm -v "$(pwd)/test:/output:z" --privileged --security-opt apparmor:unconfined --pid="host" --cap-add SYS_ADMIN --device /dev/fuse -it menne386/cloudcryptfs_fstest $@

fi

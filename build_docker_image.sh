#!/bin/bash
# Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
echo "Building fstest:"
docker run --privileged --rm -v "$(pwd)/fstest:/src" --user "$(id -u):$(id -g)" -t menne386/docker_dev 
echo "Building cloudCryptFS.docker:"
docker run --privileged --rm -v "$(pwd):/src" --user "$(id -u):$(id -g)" -t menne386/docker_dev -j5 cloudCryptFS.docker 
echo "Making menne386/cloudcryptfs_backup image:"
docker build --network host -q -t menne386/cloudcryptfs_backup . 
echo "Making menne386/cloudcryptfs_fstest image:"
docker build --network host -q -t menne386/cloudcryptfs_fstest . -f Dockerfile-fstest


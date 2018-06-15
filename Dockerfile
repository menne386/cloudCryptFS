# Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
FROM debian:stable-slim
LABEL maintainer="Menne Kamminga <kamminga.m@gmail.com>"
LABEL description="An rsync server that will encrypt data you send to it using the cloudCryptFS project"

RUN apt-get update && apt-get install -y \
    rsync \
		fuse \
		libcrypto++6 \
		libsodium23 \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

EXPOSE 873

COPY cloudCryptFS.docker entry_point.sh /


ENTRYPOINT ["/entry_point.sh"]


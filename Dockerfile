# Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
FROM debian:stable-slim
LABEL maintainer="Menne Kamminga <kamminga.m@gmail.com>"
LABEL description="A backup script that will backup items you provide into encrypted storage."

RUN apt-get update && apt-get install -y \
    rsync \
		fuse \
		libcrypto++6 \
		libsodium23 \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

COPY cloudCryptFS.docker entry_point.sh /

RUN mkdir /target && mkdir /secrets && mkdir /source
VOLUME /target
VOLUME /secrets
VOLUME /source

ENV PASSFILE="/secrets/password" KEYFILE="/secrets/keyfile.key" LOGLEVEL=1


ENTRYPOINT ["/entry_point.sh"]


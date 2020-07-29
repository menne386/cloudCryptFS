FROM debian:stable-slim

LABEL maintainer="Menne Kamminga <kamminga.m@gmail.com>" 
LABEL description="Base build environment for c/c++ apps (libfuse libcrypto++)"

RUN apt-get update && apt-get install -y \
		libfuse-dev \
		libfuse3-dev \
		g++ make binutils libcrypto++-dev libsodium-dev ;\
    apt-get clean; rm /var/lib/apt/lists/* -r; rm -rf /usr/share/man/*

RUN mkdir /src

COPY ./entry_point.sh /

ENTRYPOINT ["/entry_point.sh"]


FROM ubuntu:20.04

ARG BRANCH_OR_TAG=main
ARG CMAKE_OPTIONS=
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -qq && apt-get install -y \
    cmake \
    gcc \
    make \
    git \
    binutils \
    libc6-dev && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* && \
    rm -rf /var/cache/apk/*

ENV WORK_PATH /workspace
WORKDIR $WORK_PATH
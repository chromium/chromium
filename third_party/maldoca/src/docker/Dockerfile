# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Build the image locally:
# cd to //maldoca
# docker build -f docker/Dockerfile -t <tag> .

# Based on Ubuntu 20.04
FROM ubuntu:focal

RUN apt-get update

RUN apt-get -y dist-upgrade

# Install some utils.
RUN apt-get install -y apt-utils apt-transport-https ca-certificates curl gnupg wget software-properties-common

# Install Clang-12.
RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key| apt-key add -
RUN apt-add-repository -y "deb http://apt.llvm.org/focal/ llvm-toolchain-focal-12 main"
RUN apt-get update
# LLVM
RUN apt-get install -y libllvm-12-ocaml-dev libllvm12 llvm-12 llvm-12-dev llvm-12-doc llvm-12-examples llvm-12-runtime
# Clang and co
RUN apt-get install -y clang-12 clang-tools-12 clang-12-doc libclang-common-12-dev libclang-12-dev libclang1-12 clang-format-12 clangd-12
# libfuzzer
RUN apt-get install -y libfuzzer-12-dev
# lldb
RUN apt-get install -y lldb-12
# lld (linker)
RUN apt-get install -y lld-12
# libc++
RUN apt-get install -y libc++-12-dev libc++abi-12-dev
# symlink the clang-12 to clang
RUN ln -s /usr/bin/clang-12 /usr/bin/clang && \
  ln -s /usr/bin/clang++-12 /usr/bin/clang++ && \
  ln -s /usr/bin/clangd-12 /usr/bin/cland && \
  ln -s /usr/bin/lld-link-12 /usr/bin/lld-link && \
  ln -s /usr/bin/lld-12 /usr/bin/lld && \
  ln -s /usr/bin/clang-format-12 /usr/bin/clang-format
#RUN apt-get install -y clang

# Install Java. It is required for "bazel coverage".
RUN apt-get install -y default-jdk

# Install Bazel.
RUN curl https://bazel.build/bazel-release.pub.gpg | apt-key add -
RUN echo "deb [arch=amd64] https://storage.googleapis.com/bazel-apt stable jdk1.8" | tee /etc/apt/sources.list.d/bazel.list
RUN apt-get update
RUN apt-get install -y bazel

# Install Git.
RUN apt-get install -y git

# To use the docker for dev purposes, set DOCKER_USER to your user of choice
# via flag --build-arg DOCKER_USER=$USER and set DOCKER_UID via
# --build-arg DOCKER_UID="$(id -u)" in docker build. Optional override password.
# We need these set up to enable sudo. If DOCKER_USER build-arg is not set
# by default, the user env will not be crated and is suitable for running CI or
# other non-interactive tasks.
#
# docker build --build-arg DOCKER_UID="$(id -u)"  --build-arg DOCKER_USER=$USER  -t maldoca_build -f docker/Dockerfile .

ARG DOCKER_USER
ARG DOCKER_UID=8191
ARG DOCKER_PASS=xyzzy
ARG DOCKER_TZ='America/Los_Angeles'

# if DOCKER_USER is set then add the following
RUN if ! [ -z ${DOCKER_USER+1} ] ; then \
  ln -snf "/usr/share/zoneinfo/${DOCKER_TZ}" /etc/localtime && echo "${DOCKER_TZ}" > /etc/timezone; \
  useradd -r -m -u "${DOCKER_UID}" -s /bin/bash $DOCKER_USER; \
  echo "${DOCKER_USER}:${DOCKER_PASS}" | chpasswd && adduser "${DOCKER_USER}" sudo; \
  fi

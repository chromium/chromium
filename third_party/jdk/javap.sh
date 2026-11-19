#!/bin/sh
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Wrapper around the hermetic JDK's javap, selecting the GraalVM native-image
# on linux-x64 (including remote Siso workers running from non-Linux hosts) and
# falling back to the JVM javap elsewhere.

set -e
case "$0" in
  */*) jdk_root=${0%/*} ;;
  *) jdk_root=. ;;
esac
jdk_home="$jdk_root/current"
if [ -d "$jdk_root/linux-amd64" ]; then
  jdk_home="$jdk_root/linux-amd64"
fi
os_arch=$(uname -sm)
case "$os_arch" in
  "Linux x86_64") exec "$jdk_home/bin/jdk-tools-graalvm" javap "$@" ;;
  Darwin\ *) exec "$jdk_root/current/Contents/Home/bin/javap" "$@" ;;
  *) exec "$jdk_home/bin/javap" "$@" ;;
esac

#!/bin/bash
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -euxo pipefail

PREFIX="$1"
DEPS_PREFIX="$2"
PWD="$(pwd)"

./configure --prefix "$PREFIX" --enable-selinux=no --libdir="$PREFIX/lib64"

make install -j $(nproc)

# We need pkgconfig file to point to location where it's going to be deployed
# and not where it was installed
sed "s@$PREFIX@$DEPS_PREFIX@" -i "$PREFIX/lib64/pkgconfig/dbus-1.pc"

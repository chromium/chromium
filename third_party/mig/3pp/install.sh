#!/bin/bash
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -euxo pipefail

PREFIX="$1"

mkdir -p installdir

autoreconf --install
./configure --prefix="$PWD/installdir"
make
make install

mkdir -p "$PREFIX"
cp installdir/bin/mig "$PREFIX"
cp installdir/bin/migcom "$PREFIX"
strip "$PREFIX/migcom"

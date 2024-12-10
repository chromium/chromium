#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -euxo pipefail

echo "Building on the following system: "
uname -a
uname -r

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

PREFIX="$1"
DEPS_PREFIX="$2"
PWD="$(pwd)"

mkdir -p helper_pkgs
HELPER_PKGS_PATH="$PWD/helper_pkgs"

echo "Installing dependencies to build libei"

# Will install into /work/tools_prefix, since that's where Python is.
python3 -m pip install --upgrade pip
python3 -m pip install meson
python3 -m pip install attrs
python3 -m pip install jinja2

export PATH="$HELPER_PKGS_PATH/bin:$PATH"

echo "Installing rsync (needed by linux headers)"
tar xf v3.2.7.tar.gz
cd rsync-3.2.7
./configure --prefix=${HELPER_PKGS_PATH} \
  --disable-md2man \
  --disable-openssl \
  --disable-xxhash \
  --disable-zstd \
  --disable-lz4
make install -j$(nproc)
cd ..

echo "Installing linux headers (libei depends on input-event-code.h)"
tar xf linux-5.4.114.tar.gz
cd linux-5.4.114
make headers_install INSTALL_HDR_PATH="$HELPER_PKGS_PATH"
cd ..

echo "Building libei"
tar xf libei-*.tar.gz
cd libei-*/

echo "Compiler details (including search paths):"
`gcc -print-prog-name=cpp` -v

meson setup -D c_args="-I$HELPER_PKGS_PATH/include" \
  -D c_link_args="-lm"  -D liboeffis=disabled build . --default-library=static \
  --prefix=$PREFIX --includedir=include
meson compile -C build --verbose
meson install -C build --no-rebuild

# pkgconfig files point into the working directory, and we don't need them
# anyway.
rm -R "$PREFIX/lib64/pkgconfig"
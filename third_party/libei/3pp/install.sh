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

# libei can be only build using Meson. We need Meson only to build libei
# so it doesn't have to be uploaded to CIPD.
export PYTHONPATH="$HELPER_PKGS_PATH/lib64/python3.6/site-packages:$HELPER_PKGS_PATH/lib/python3.6/site-packages/"

echo "Installing dependencies to build libei"

# Meson 0.62 and newer will require Python 3.7 and newer and Ubuntu 18.04 has
# only Python 3.6
python3 -m pip install meson==0.61.5 --prefix="$HELPER_PKGS_PATH"

export PKG_CONFIG_PATH="$DEPS_PREFIX/lib64/pkgconfig"
export PATH="$PATH:$HELPER_PKGS_PATH:$HELPER_PKGS_PATH/bin"

# Install required dependencies
python3 -m pip install --upgrade pip --prefix="$HELPER_PKGS_PATH"
python3 -m pip install cmake --prefer-binary --prefix="$HELPER_PKGS_PATH"
python3 -m pip install python-dbusmock --prefix="$HELPER_PKGS_PATH"
python3 -m pip install attr --prefix="$HELPER_PKGS_PATH"
python3 -m pip install pytest -v --prefer-binary --prefix="$HELPER_PKGS_PATH" 2>&1
python3 -m pip install structlog --prefix="$HELPER_PKGS_PATH"
python3 -m pip install jinja2 --prefix="$HELPER_PKGS_PATH"

echo "Installing MarkupSafe (a dependency for jinja2)"
tar xf MarkupSafe-2.1.2.tar.gz
cd MarkupSafe-2.1.2
python3 setup.py install --prefix="$HELPER_PKGS_PATH"
cd ..

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
# make headers_install INSTALL_HDR_PATH=/usr
make headers_install INSTALL_HDR_PATH=$PREFIX
cd ..

echo "Building libei"
tar xf libei-main.tar.gz
cd libei-main

echo "Compiler details (including search paths):"
`gcc -print-prog-name=cpp` -v

$HELPER_PKGS_PATH/bin/meson setup -D c_args="-I$PREFIX/include" \
  -D c_link_args="-lm" build . --default-library=static \
  --prefix=$PREFIX --includedir=include
$HELPER_PKGS_PATH/bin/meson --buildtype=plain --prefix="$PREFIX" . build \
  -D docs=disabled -D man=disabled
$HELPER_PKGS_PATH/bin/meson compile -C build --verbose
$HELPER_PKGS_PATH/bin/meson install -C build --no-rebuild

# We need pkgconfig file to point to location where it's going to be deployed
# and not where it was installed
sed "s@$PREFIX@$DEPS_PREFIX@" -i "$PREFIX/lib64/pkgconfig/libei.pc"

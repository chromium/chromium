#!/bin/bash

# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script for downloading some WebUI's NPM deps without using `npm install`.
# Unfortunately `npm install` downloads a lot of unnecessary dependencies in
# many cases. This is especially true for cases like 'mocha'and 'chai', which
# even though come already bundled, NPM still downloads about ~8MB of
# unnecessary dependencies.
#
# To address the problem above and avoid blowing up the size of the generated
# node_modules.tar.gz file, this script directly downloads just the necessary
# packages, since NPM does not provide such functionality built-in, see
# https://github.com/npm/npm/issues/340.

set -eu

tmp_dir="node_modules_manual"
mkdir -p "${tmp_dir}"

CHAI_VERSION="5.1.1"
MOCHA_VERSION="10.4.0"

download_package() {
  local PACKAGE="$1"
  local VERSION="$2"

  # Manually download from NPM.
  npm pack --silent ${PACKAGE}@${VERSION} --pack-destination ${tmp_dir}

  # Extract.
  tar xfz ${tmp_dir}/${PACKAGE}-${VERSION}.tgz -C ${tmp_dir}

  # Delete destination folder if it already exists.
  rm -rf node_modules/${PACKAGE}

  # Place in node_modules/ folder. Any further filtering of files happens in the
  # parent script.
  mv ${tmp_dir}/package node_modules/${PACKAGE}
}

download_package "chai" ${CHAI_VERSION}
rm -rf ${tmp_dir}/package
download_package "mocha" ${MOCHA_VERSION}

# Clean up.
rm -rf "$tmp_dir"

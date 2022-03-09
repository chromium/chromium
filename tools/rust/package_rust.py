#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import shutil
import sys
import tarfile

THIS_DIR = os.path.dirname(__file__)
sys.path.append(
    os.path.join(os.path.dirname(THIS_DIR), '..', 'clang', 'scripts'))

from build_rust import (PACKAGE_VERSION, RUST_TOOLCHAIN_OUT_DIR,
                        THIRD_PARTY_DIR, VERSION_STAMP_PATH)
from package import (MaybeUpload, TeeCmd)
from update import (CHROMIUM_DIR)

BUILDLOG_NAME = f'rust-buildlog-{PACKAGE_VERSION}.txt'
RUST_TOOLCHAIN_PACKAGE_NAME = f'rust-toolchain-{PACKAGE_VERSION}.tgz'


def main():
  parser = argparse.ArgumentParser(description='build and package Rust')
  parser.add_argument('--upload',
                      action='store_true',
                      help='upload package to GCS')
  args = parser.parse_args()

  # Only build on Linux. Other platforms are currently unsupported.
  if not sys.platform.startswith('linux'):
    print('Only Linux is supported!')
    return 1

  gcs_platform = 'Linux_x64'

  # Clean build output directory
  if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
    shutil.rmtree(RUST_TOOLCHAIN_OUT_DIR)

  with open(os.path.join(THIRD_PARTY_DIR, BUILDLOG_NAME), 'w') as log:
    build_cmd = [sys.executable, os.path.join(THIS_DIR, 'build_rust.py')]
    TeeCmd(build_cmd, log)

  with tarfile.open(os.path.join(THIRD_PARTY_DIR, RUST_TOOLCHAIN_PACKAGE_NAME),
                    'w:gz') as tar:
    tar.add(RUST_TOOLCHAIN_OUT_DIR, arcname='rust-toolchain')

  os.chdir(THIRD_PARTY_DIR)
  MaybeUpload(args.upload, RUST_TOOLCHAIN_PACKAGE_NAME, gcs_platform)
  MaybeUpload(args.upload, BUILDLOG_NAME, gcs_platform)

  return 0


if __name__ == '__main__':
  sys.exit(main())

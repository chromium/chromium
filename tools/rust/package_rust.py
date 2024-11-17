#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import lzma
import os
import platform
import shutil
import subprocess
import sys
import tarfile

THIS_DIR = os.path.dirname(__file__)
sys.path.append(
    os.path.join(os.path.dirname(THIS_DIR), '..', 'clang', 'scripts'))

from build_rust import (RUST_TOOLCHAIN_OUT_DIR, THIRD_PARTY_DIR)
from update_rust import (GetRustClangRevision)
from package import (MaybeUpload, TeeCmd, DEFAULT_GCS_BUCKET)
from update import (CHROMIUM_DIR)

PACKAGE_VERSION = GetRustClangRevision()
BUILDLOG_NAME = f'rust-buildlog-{PACKAGE_VERSION}.txt'
RUST_TOOLCHAIN_PACKAGE_NAME = f'rust-toolchain-{PACKAGE_VERSION}.tar.xz'


def main():
    parser = argparse.ArgumentParser(description='build and package Rust')
    parser.add_argument('--upload',
                        action='store_true',
                        help='upload package to GCS')
    parser.add_argument(
        '--bucket',
        default=DEFAULT_GCS_BUCKET,
        help='Google Cloud Storage bucket where the target archive is uploaded'
    )
    args = parser.parse_args()

    # The gcs_platform logic copied from `//tools/clang/scripts/upload.sh`.
    if sys.platform == 'darwin':
        if platform.machine() == 'arm64':
            gcs_platform = 'Mac_arm64'
        else:
            gcs_platform = 'Mac'
    elif sys.platform == 'win32':
        gcs_platform = 'Win'
    else:
        gcs_platform = 'Linux_x64'

    # Clean build output directory.
    if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
        shutil.rmtree(RUST_TOOLCHAIN_OUT_DIR)

    with open(os.path.join(THIRD_PARTY_DIR, BUILDLOG_NAME),
              'w',
              encoding='utf-8') as log:
        # Build the Rust toolchain.
        build_cmd = [sys.executable, os.path.join(THIS_DIR, 'build_rust.py')]
        TeeCmd(build_cmd, log)

        # Build bindgen.
        build_cmd = [
            sys.executable,
            os.path.join(THIS_DIR, 'build_bindgen.py')
        ]
        TeeCmd(build_cmd, log)

        # Build cargo-vet.
        build_cmd = [sys.executable, os.path.join(THIS_DIR, 'build_vet.py')]
        TeeCmd(build_cmd, log)

        # Build Crubit.
        build_cmd = [sys.executable, os.path.join(THIS_DIR, 'build_crubit.py')]
        # TODO: crbug.com/40226863 - Remove `fail_hard=False` once we can depend
        # on the OSS Crubit build staying green with latest Rust and Clang.
        TeeCmd(build_cmd, log, fail_hard=False)

    # Strip everything in bin/ to reduce the package size.
    bin_dir_path = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'bin')
    if sys.platform != 'win32' and os.path.exists(bin_dir_path):
        for f in os.listdir(bin_dir_path):
            file_path = os.path.join(bin_dir_path, f)
            if not os.path.islink(file_path):
                subprocess.call(['strip', file_path])

    with tarfile.open(os.path.join(THIRD_PARTY_DIR,
                                   RUST_TOOLCHAIN_PACKAGE_NAME),
                      'w:xz',
                      preset=9 | lzma.PRESET_EXTREME) as tar:
        for f in sorted(os.listdir(RUST_TOOLCHAIN_OUT_DIR)):
            tar.add(os.path.join(RUST_TOOLCHAIN_OUT_DIR, f), arcname=f)

    os.chdir(THIRD_PARTY_DIR)
    MaybeUpload(args.upload, args.bucket, RUST_TOOLCHAIN_PACKAGE_NAME,
                gcs_platform)
    MaybeUpload(args.upload, args.bucket, BUILDLOG_NAME, gcs_platform)

    return 0


if __name__ == '__main__':
    sys.exit(main())

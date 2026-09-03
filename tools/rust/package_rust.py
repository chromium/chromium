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
    os.path.join(os.path.dirname(THIS_DIR), '..', 'clang', 'scripts')
)

from build_rust import (
    RUST_TOOLCHAIN_OUT_DIR,
    THIRD_PARTY_DIR,
    RUST_HOST_LLVM_INSTALL_DIR,
)
from update_rust import GetRustClangRevision
from package import (
    DEFAULT_GCS_BUCKET,
    MaybeUpload,
    TeeCmd,
    VerifyPackageDoesntExist,
)
from update import CHROMIUM_DIR

PACKAGE_VERSION = GetRustClangRevision()
BUILDLOG_NAME = f'rust-buildlog-{PACKAGE_VERSION}.txt'
RUST_TOOLCHAIN_PACKAGE_NAME = f'rust-toolchain-{PACKAGE_VERSION}.tar.xz'
RUST_LIBCLANG_PACKAGE_NAME = f'rust-libclang-{PACKAGE_VERSION}.tar.xz'


def main():
    parser = argparse.ArgumentParser(description='build and package Rust')
    parser.add_argument(
        '--upload', action='store_true', help='upload package to GCS'
    )
    parser.add_argument(
        '--bucket',
        default=DEFAULT_GCS_BUCKET,
        help='Google Cloud Storage bucket where the target archive is uploaded',
    )
    parser.add_argument(
        '--skip-test',
        action='store_true',
        help='skip running rustc and libstd tests',
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

    if args.upload:
        VerifyPackageDoesntExist(
            args.bucket, RUST_TOOLCHAIN_PACKAGE_NAME, gcs_platform
        )

    # Clean build output directory.
    if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
        shutil.rmtree(RUST_TOOLCHAIN_OUT_DIR)

    with open(
        os.path.join(THIRD_PARTY_DIR, BUILDLOG_NAME), 'w', encoding='utf-8'
    ) as log:
        # Build the Rust toolchain, bindgen and crubit.
        build_cmd = [
            sys.executable,
            os.path.join(THIS_DIR, 'build_rust.py'),
            '--build-bindgen',
            '--build-crubit',
        ]
        if args.skip_test:
            build_cmd.append('--skip-test')
        TeeCmd(build_cmd, log)

    # Strip everything in bin/ to reduce the package size.
    print('Stripping binaries to reduce the package size ...')
    bin_dir_path = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'bin')
    if sys.platform != 'win32' and os.path.exists(bin_dir_path):
        for f in os.listdir(bin_dir_path):
            file_path = os.path.join(bin_dir_path, f)
            if not os.path.islink(file_path):
                print(f'    Stripping {f} ...')
                subprocess.call(['strip', file_path])

    # Create main archive.
    print(f'Creating a tar package {RUST_TOOLCHAIN_PACKAGE_NAME}...')
    with tarfile.open(
        os.path.join(THIRD_PARTY_DIR, RUST_TOOLCHAIN_PACKAGE_NAME),
        'w:xz',
        preset=9 | lzma.PRESET_EXTREME,
    ) as tar:
        for f in sorted(os.listdir(RUST_TOOLCHAIN_OUT_DIR)):
            tar.add(os.path.join(RUST_TOOLCHAIN_OUT_DIR, f), arcname=f)
        for f in tar.getnames():
            print(f'    Packaged {f}')

    # Create libclang archive.
    print(f'Creating a tar package {RUST_LIBCLANG_PACKAGE_NAME}...')
    with tarfile.open(
        os.path.join(THIRD_PARTY_DIR, RUST_LIBCLANG_PACKAGE_NAME),
        'w:xz',
        preset=9 | lzma.PRESET_EXTREME,
        dereference=True,
    ) as tar:
        want = []
        if sys.platform == 'darwin':
            want.append(os.path.join('lib', 'libclang.dylib'))
        elif sys.platform == 'win32':
            want.append(os.path.join('lib', 'libclang.lib'))
            want.append(os.path.join('bin', 'libclang.dll'))
        else:
            want.append(os.path.join('lib', 'libclang.so'))
        include_dir = os.path.join(
            RUST_HOST_LLVM_INSTALL_DIR, 'include', 'clang-c'
        )
        for f in sorted(os.listdir(include_dir)):
            want.append(os.path.join('include', 'clang-c', f))
        for f in want:
            tar.add(os.path.join(RUST_HOST_LLVM_INSTALL_DIR, f), arcname=f)

        pydir = os.path.join(THIRD_PARTY_DIR, 'llvm', 'clang')
        pyfiles = [
            os.path.join('bindings', 'python', 'clang', 'cindex.py'),
            os.path.join('bindings', 'python', 'clang', '__init__.py'),
        ]
        for f in pyfiles:
            tar.add(os.path.join(pydir, f), arcname=f)

        for f in tar.getnames():
            print(f'    Packaged {f}')

    os.chdir(THIRD_PARTY_DIR)
    MaybeUpload(
        args.upload, args.bucket, RUST_TOOLCHAIN_PACKAGE_NAME, gcs_platform
    )
    MaybeUpload(
        args.upload, args.bucket, RUST_LIBCLANG_PACKAGE_NAME, gcs_platform
    )
    MaybeUpload(args.upload, args.bucket, BUILDLOG_NAME, gcs_platform)

    return 0


if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Update in-tree checkout of Rust toolchain

!!! DO NOT USE IN PRODUCTION
Some functionality can be used outside of a chromium checkout. For example,
running with `--print-rust-revision` will succeed. Other functionality requires
a Chromium checkout to access functions from other scripts.

'''

import argparse
import os
import re
import shutil
import sys
import tempfile
import urllib

from pathlib import Path

# Add Clang scripts to path so we can import them later (if running within a
# Chromium checkout.)
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'clang',
                 'scripts'))

RUST_REVISION = '20221209'
RUST_SUB_REVISION = 1

# Trunk on 2022-10-15.
#
# The revision specified below should typically be the same as the
# `crubit_revision` specified in the //DEPS file.  More details and roll
# instructions can be found in tools/rust/README.md.
CRUBIT_REVISION = 'f5cbdf4b54b0e6b9f63a4464a2c901c82e0f0209'
CRUBIT_SUB_REVISION = 1

# TODO(crbug.com/1401042): Set this back to None once Clang rolls block on Rust
# building. Until Clang rolls block on Rust, they frequently roll without a
# Rust compiler, which causes developer machines/bots to 404 in gclient sync.
#
# If not None, use a Rust package built with an older LLVM version than
# specified in tools/clang/scripts/update.py. This is a fallback for when an
# LLVM update breaks the Rust build.
#
# This should almost always be None. When a breakage happens the fallback should
# be temporary. Once fixed, the applicable revision(s) above should be updated
# and FALLBACK_CLANG_VERSION should be reset to None.
FALLBACK_CLANG_VERSION = 'llvmorg-16-init-13328-g110fe4f4-1'

# Hash of src/stage0.json, which itself contains the stage0 toolchain hashes.
# We trust the Rust build system checks, but to ensure it is not tampered with
# itself check the hash.
STAGE0_JSON_SHA256 = (
    '07f4d4ddde6910a70f16f372309525528ff42499fb50317e6ded4bfe1b6ce7cf')

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..'))
THIRD_PARTY_DIR = os.path.join(CHROMIUM_DIR, 'third_party')
RUST_TOOLCHAIN_OUT_DIR = os.path.join(THIRD_PARTY_DIR, 'rust-toolchain')
VERSION_STAMP_PATH = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'VERSION')


# Get the package version for the RUST_[SUB_]REVISION above with the specified
# clang_version.
def GetPackageVersion(clang_version):
    # TODO(lukasza): Include CRUBIT_REVISION and CRUBIT_SUB_REVISION once we
    # include Crubit binaries in the generated package.  See also a TODO comment
    # in BuildCrubit in package_rust.py.
    return f'{RUST_REVISION}-{RUST_SUB_REVISION}-{clang_version}'


# Package version built in build_rust.py, which is always built against the
# latest Clang and never uses the FALLBACK_CLANG_VERSION.
def GetPackageVersionForBuild():
    from update import (CLANG_REVISION, CLANG_SUB_REVISION)
    return GetPackageVersion(f'{CLANG_REVISION}-{CLANG_SUB_REVISION}')


# Package version for download, which may differ from GetUploadPackageVersion()
# if FALLBACK_CLANG_VERSION is set.
def GetDownloadPackageVersion():
    if FALLBACK_CLANG_VERSION:
        return GetPackageVersion(FALLBACK_CLANG_VERSION)
    else:
        return GetPackageVersionForBuild()


# Get the version of the toolchain package we already have.
def GetStampVersion():
    if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
        with open(VERSION_STAMP_PATH) as version_file:
            existing_stamp = version_file.readline().rstrip()
        version_re = re.compile(
            r'rustc [0-9.]+-nightly \([0-9a-f -]+\) \((.+?) chromium\)')
        match = version_re.fullmatch(existing_stamp)
        if match is None:
            return None
        return match.group(1)

    return None


def main():
    parser = argparse.ArgumentParser(description='Update Rust package')
    parser.add_argument(
        '--print-rust-revision',
        action='store_true',
        help='Print Rust revision (without Clang revision) and '
        'quit. Can be run outside of a Chromium checkout.')
    parser.add_argument('--print-package-version',
                        action='store_true',
                        help='Print Rust package version (including both the '
                        'Rust and Clang revisions) and quit.')
    args = parser.parse_args()

    if args.print_rust_revision:
        print(f'{RUST_REVISION}-{RUST_SUB_REVISION}')
        return 0

    if args.print_package_version:
        print(GetDownloadPackageVersion())
        return 0

    from update import (DownloadAndUnpack, GetDefaultHostOs,
                        GetPlatformUrlPrefix)

    # Exit early if the existing package is up-to-date. Note that we cannot
    # simply call DownloadAndUnpack() every time: aside from unnecessarily
    # downloading the toolchain if it hasn't changed, it also leads to multiple
    # versions of the same rustlibs. build/rust/std/find_std_rlibs.py chokes in
    # this case.
    if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
        if GetDownloadPackageVersion() == GetStampVersion():
            return 0

    if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
        shutil.rmtree(RUST_TOOLCHAIN_OUT_DIR)

    try:
        platform_prefix = GetPlatformUrlPrefix(GetDefaultHostOs())
        version = GetDownloadPackageVersion()
        url = f'{platform_prefix}rust-toolchain-{version}.tgz'
        DownloadAndUnpack(url, THIRD_PARTY_DIR)
    except urllib.error.HTTPError as e:
        # Fail softly for now. This can happen if a Rust package was not
        # produced, e.g. if the Rust build failed upon a Clang update, or if a
        # Rust roll and a Clang roll raced against each other.
        #
        # TODO(https://crbug.com/1245714): Reconsider how to handle this.
        print(f'warning: could not download Rust package')

    # Ensure the newly extracted package has the correct version.
    assert GetDownloadPackageVersion() == GetStampVersion()


if __name__ == '__main__':
    sys.exit(main())

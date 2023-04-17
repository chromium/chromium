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
import time
import urllib

from pathlib import Path

# Add Clang scripts to path so we can import them later (if running within a
# Chromium checkout.)
# Note: Imports cannot be done until after the --print-rust-revision flag
# has been processed, since that needs to work when running this script
# in isolation.
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'clang',
                 'scripts'))

# These fields are written by //tools/clang/scripts/upload_revision.py, and
# should not be changed manually.
RUST_REVISION = '17c11672167827b0dd92c88ef69f24346d1286dd'
RUST_SUB_REVISION = 1

# Trunk on 2022-10-15.
#
# The revision specified below should typically be the same as the
# `crubit_revision` specified in the //DEPS file.  More details and roll
# instructions can be found in tools/rust/README.md.
#
# TODO(danakj): This should be included in --print-rust-revision when we want
# code to depend on using crubit rs_to_cc_bindings.
CRUBIT_REVISION = 'f5cbdf4b54b0e6b9f63a4464a2c901c82e0f0209'
CRUBIT_SUB_REVISION = 1

# TODO(crbug.com/1401042): Set this back to None once Clang rolls block on Rust
# building. Until Clang rolls block on Rust, they frequently roll without a Rust
# compiler, which causes developer machines/bots to 404 in gclient sync.
#
# If not None, use a Rust package built with an older LLVM version than
# specified in tools/clang/scripts/update.py. This is a fallback for when an
# LLVM update breaks the Rust build.
#
# This should almost always be None. When a breakage happens the fallback should
# be temporary. Once fixed, the applicable revision(s) above should be updated
# and FALLBACK_REVISION should be reset to None.
#
# Rust builds (for Linux) that worked are found at:
# https://commondatastorage.googleapis.com/chromium-browser-clang/index.html?path=Linux_x64/rust-toolchain-
# The latest builds are prefixed with a date, such as `20230101-1`.
#
# TODO(lukasza): Include CRUBIT_REVISION and CRUBIT_SUB_REVISION once we
# include Crubit binaries in the generated package.  See also a TODO comment
# in BuildCrubit in package_rust.py.
FALLBACK_REVISION = '17c11672167827b0dd92c88ef69f24346d1286dd-1-llvmorg-17-init-8029-g27f27d15-1'

# Hash of src/stage0.json, which itself contains the stage0 toolchain hashes.
# We trust the Rust build system checks, but to ensure it is not tampered with
# itself check the hash.
STAGE0_JSON_SHA256 = '7339c4d847db37e8c0b69b5c037bb708fcce1baf22ac5bf598dc739e559bcbc1'

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..'))
THIRD_PARTY_DIR = os.path.join(CHROMIUM_DIR, 'third_party')
RUST_TOOLCHAIN_OUT_DIR = os.path.join(THIRD_PARTY_DIR, 'rust-toolchain')
VERSION_STAMP_PATH = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'VERSION')


# Package version built in build_rust.py, which is always built against the
# current Clang. Typically Clang and Rust revisions are both updated together
# and this picks the Clang that has just been built.
def GetLatestRevision():
    from update import (CLANG_REVISION, CLANG_SUB_REVISION)
    return (f'{RUST_REVISION}-{RUST_SUB_REVISION}'
            f'-{CLANG_REVISION}-{CLANG_SUB_REVISION}')


# Package version for download. Ideally this is the latest Clang+Rust roll,
# which was built successfully and is returned from GetLatestRevision().
# However at this time Clang rolls even if Rust fails to build, so we have Rust
# pinned to the last known successful build with FALLBACK_REVISION. This should
# go away once we block Clang rolls on Rust also being built.
def GetDownloadPackageVersion():
    return FALLBACK_REVISION \
        if FALLBACK_REVISION else GetLatestRevision()


# Get the version of the toolchain package we already have.
def GetStampVersion():
    if os.path.exists(VERSION_STAMP_PATH):
        with open(VERSION_STAMP_PATH) as version_file:
            existing_stamp = version_file.readline().rstrip()
        version_re = re.compile(r'rustc [0-9.]+ [0-9a-f]+ \((.+?) chromium\)')
        match = version_re.fullmatch(existing_stamp)
        if match is None:
            return None
        return match.group(1)

    return None


def CheckUrl(url) -> bool:
    """Check if a url exists."""
    num_retries = 3
    retry_wait_s = 5  # Doubled at each retry.

    while True:
        try:
            urllib.request.urlopen(urllib.request.Request(url))
            return True
        except urllib.error.URLError as e:
            if e.code != 404:
                print(e)
            if num_retries == 0 or isinstance(
                    e, urllib.error.HTTPError) and e.code == 404:
                return False
            num_retries -= 1
            print(f'Retrying in {retry_wait_s} s ...')
            sys.stdout.flush()
            time.sleep(retry_wait_s)
            retry_wait_s *= 2


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
        stamp_version = GetStampVersion()
        if (stamp_version != GetLatestRevision()
                and stamp_version != FALLBACK_REVISION):
            print(
                f'The expected Rust version is {GetLatestRevision()} '
                f'(or fallback {FALLBACK_REVISION} but the actual version is '
                f'{stamp_version}')
            print('Did you run "gclient sync"?')
            return 1
        print(stamp_version)
        return 0

    from update import (DownloadAndUnpack, GetDefaultHostOs,
                        GetPlatformUrlPrefix)

    platform_prefix = GetPlatformUrlPrefix(GetDefaultHostOs())

    version = GetLatestRevision()
    url = f'{platform_prefix}rust-toolchain-{version}.tgz'
    if not CheckUrl(url):
        print("Latest Rust toolchain not found. Using fallback revision.")
        version = FALLBACK_REVISION
        url = f'{platform_prefix}rust-toolchain-{version}.tgz'
        if not CheckUrl(url):
            print('error: Could not find Rust toolchain package')
            return 1

    # Exit early if the existing package is up-to-date. Note that we cannot
    # simply call DownloadAndUnpack() every time: aside from unnecessarily
    # downloading the toolchain if it hasn't changed, it also leads to multiple
    # versions of the same rustlibs. build/rust/std/find_std_rlibs.py chokes in
    # this case.
    if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
        if version == GetStampVersion():
            return 0

    if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
        shutil.rmtree(RUST_TOOLCHAIN_OUT_DIR)

    try:
        platform_prefix = GetPlatformUrlPrefix(GetDefaultHostOs())
        DownloadAndUnpack(url, THIRD_PARTY_DIR)
    except urllib.error.HTTPError as e:
        print(f'error: Failed to download Rust package')
        return 1

    # Ensure the newly extracted package has the correct version.
    assert version == GetStampVersion()


if __name__ == '__main__':
    sys.exit(main())

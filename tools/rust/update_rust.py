#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Update in-tree checkout of Rust toolchain

When run without arguments, it fetches and unzips the Rust toolchain package
specieid by the `RUST_REVISION` and `RUST_SUB_REVISION` along with the clang
version specified in //tools/clang/scripts/update.py.

Specify --output-dir to override the location for the Rust toolchain package,
which otherwise defaults to //third_party/rust-toolchain.
(Note that the output dir may be deleted and re-created if it exists.)
'''

import argparse
import glob
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
# They are also read by build/config/compiler/BUILD.gn.
RUST_REVISION = '11339a0ef5ed586bb7ea4f85a9b7287880caac3a'
RUST_SUB_REVISION = 1

# The revision of Crubit to use from https://github.com/google/crubit
#
# If changing the CRUBIT_REVISION but not the RUST_REVISION, bump the
# RUST_SUB_REVISION to generate a unique package name.
CRUBIT_REVISION = 'fa6caca0969c9d1dec584186eb85ebdd0fe02955'
# The Absl revision used for building Crubit. Can be bumped to the latest when
# rolling Crubit. There's no reason to change this if not rolling Crubit.
ABSL_REVISION = 'ba5fd0979b4e74bd4d1b8da1d84347173bd9f17f'

# Hash of src/stage0.json, which itself contains the stage0 toolchain hashes.
# We trust the Rust build system checks, but to ensure it is not tampered with
# itself check the hash.
STAGE0_JSON_SHA256 = '29ce1e9f8c9477899d39a276b0341ab787a63814e6ae0df948a6de47ec153b0b'

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..'))
THIRD_PARTY_DIR = os.path.join(CHROMIUM_DIR, 'third_party')
RUST_TOOLCHAIN_OUT_DIR = os.path.join(THIRD_PARTY_DIR, 'rust-toolchain')
# Filename and path to the VERSION file stored in the archive.
VERSION_SRC_FILENAME = 'VERSION'
VERSION_SRC_PATH = os.path.join(RUST_TOOLCHAIN_OUT_DIR, VERSION_SRC_FILENAME)


def GetRustClangRevision():
    from update import CLANG_REVISION
    return f'{RUST_REVISION}-{RUST_SUB_REVISION}-{CLANG_REVISION}'


# Get the version of the toolchain package we already have.
def GetStampVersion():
    if os.path.exists(VERSION_SRC_PATH):
        with open(VERSION_SRC_PATH) as version_file:
            existing_stamp = version_file.readline().rstrip()
        version_re = re.compile(r'rustc [0-9.]+ [0-9a-f]+ \((.+?) chromium\)')
        match = version_re.fullmatch(existing_stamp)
        if match is None:
            return None
        return match.group(1)

    return None


def main():
    parser = argparse.ArgumentParser(
        description='Update Rust package',
        formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument(
        '--print-revision',
        choices=['rust', 'installed', 'validate'],
        help='Print the rust revision then quit. Possible formats:\n'
        '- rust: print only the expected rust revision (without clang).\n'
        '  Can be run outside of a Chromium checkout.\n'
        '- installed: print the installed package version (including both\n'
        '  rust and clang revisions), without checking that it matches the\n'
        '  expected version in this file.\n'
        '- validate: print the expected package version, and ensure it\n'
        '  matches the installed package.')
    parser.add_argument('--output-dir', help='Where to extract the package.')

    args = parser.parse_args()

    if args.print_revision == 'rust':
        print(f'{RUST_REVISION}-{RUST_SUB_REVISION}')
        return 0
    elif args.print_revision:
        stamp_version = GetStampVersion()
        if (args.print_revision == 'validate'
                and stamp_version != GetRustClangRevision()):
            print(f'The expected Rust version is {GetRustClangRevision()} '
                  f'but the actual version is {stamp_version}')
            print('Did you run "gclient sync"?')
            return 1
        print(stamp_version)
        return 0

    output_dir = RUST_TOOLCHAIN_OUT_DIR
    if args.output_dir:
        global VERSION_SRC_PATH
        output_dir = os.path.abspath(args.output_dir)
        VERSION_SRC_PATH = os.path.join(output_dir, VERSION_SRC_FILENAME)

    from update import (DownloadAndUnpack, GetDefaultHostOs,
                        GetPlatformUrlPrefix)

    platform_prefix = GetPlatformUrlPrefix(GetDefaultHostOs())

    version = GetRustClangRevision()

    # Exit early if the existing package is up-to-date. Note that we cannot
    # simply call DownloadAndUnpack() every time: aside from unnecessarily
    # downloading the toolchain if it hasn't changed, it also leads to multiple
    # versions of the same rustlibs. build/rust/std/find_std_rlibs.py chokes in
    # this case.
    # .*_is_first_class_gcs file is created by first class GCS deps when rust
    # hooks are migrated to be first class deps. In case we need to go back to
    # using a hook, this file will indicate that the previous download was
    # from the first class dep and the dir needs to be cleared.
    if os.path.exists(output_dir):
        if version == GetStampVersion() and not glob.glob(
                os.path.join(output_dir, '.*_is_first_class_gcs')):
            return 0

    if os.path.exists(output_dir):
        shutil.rmtree(output_dir)

    try:
        url = f'{platform_prefix}rust-toolchain-{version}.tar.xz'
        DownloadAndUnpack(url, output_dir)
    except urllib.error.HTTPError as e:
        print(f'error: Failed to download Rust package')
        return 1

    # Ensure the newly extracted package has the correct version.
    assert version == GetStampVersion()


if __name__ == '__main__':
    sys.exit(main())

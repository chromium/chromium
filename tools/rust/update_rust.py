#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
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

RUST_REVISION = 'f001f930'
RUST_SUB_REVISION = 1

# Hash of src/stage0.json, which itself contains the stage0 toolchain hashes.
# We trust the Rust build system checks, but to ensure it is not tampered with
# itself check the hash.
STAGE0_JSON_SHA256 = (
    '6b1c61d494ad447f41c8ae3b9b3239626eecac00e0f0b793b844e0761133dc37')

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..'))
THIRD_PARTY_DIR = os.path.join(CHROMIUM_DIR, 'third_party')
RUST_TOOLCHAIN_OUT_DIR = os.path.join(THIRD_PARTY_DIR, 'rust-toolchain')
VERSION_STAMP_PATH = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'VERSION')


# Get the target version as specified above.
def GetPackageVersion():
  from update import (CLANG_REVISION, CLANG_SUB_REVISION)
  return '%s-%s-%s-%s' % (RUST_REVISION, RUST_SUB_REVISION, CLANG_REVISION,
                          CLANG_SUB_REVISION)


# Get the version of the toolchain package we already have.
def GetStampVersion():
  if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
    with open(VERSION_STAMP_PATH) as version_file:
      existing_stamp = version_file.readline().rstrip()
    version_re = re.compile(r'rustc [0-9.]+-dev \((.+?) chromium\)')
    return version_re.fullmatch(existing_stamp).group(1)

  return None


def main():
  parser = argparse.ArgumentParser(description='Update Rust package')
  parser.add_argument('--print-rust-revision',
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
    print(GetPackageVersion())
    return 0

  from update import (DownloadAndUnpack, GetDefaultHostOs, GetPlatformUrlPrefix)

  # Exit early if the existing package is up-to-date. Note that we cannot simply
  # call DownloadAndUnpack() every time: aside from unnecessarily downloading
  # the toolchain if it hasn't changed, it also leads to multiple versions of
  # the same rustlibs. build/rust/std/find_std_rlibs.py chokes in this case.
  if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
    if GetPackageVersion() == GetStampVersion():
      return 0

  if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
    shutil.rmtree(RUST_TOOLCHAIN_OUT_DIR)

  try:
    url = '%srust-toolchain-%s.tgz' % (GetPlatformUrlPrefix(
        GetDefaultHostOs()), GetPackageVersion())
    DownloadAndUnpack(url, THIRD_PARTY_DIR)
  except urllib.error.HTTPError as e:
    # Fail softly for now. This can happen if a Rust package was not produced,
    # e.g. if the Rust build failed upon a Clang update, or if a Rust roll and
    # a Clang roll raced against each other.
    #
    # TODO(https://crbug.com/1245714): reconsider how to handle this.
    print(f'warning: could not download Rust package')

  # Ensure the newly extracted package has the correct version.
  assert GetPackageVersion() == GetStampVersion()


if __name__ == '__main__':
  sys.exit(main())

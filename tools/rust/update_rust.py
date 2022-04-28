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
import sys
import tempfile
import urllib

from pathlib import Path

# Add Clang scripts to path so we can import them later (if running within a
# Chromium checkout.)
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'clang',
                 'scripts'))

RUST_REVISION = '1f631e8e'
RUST_SUB_REVISION = 2

# Hash of src/stage0.json, which itself contains the stage0 toolchain hashes.
# We trust the Rust build system checks, but to ensure it is not tampered with
# itself check the hash.
STAGE0_JSON_SHA256 = (
    '6b1c61d494ad447f41c8ae3b9b3239626eecac00e0f0b793b844e0761133dc37')

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..'))
THIRD_PARTY_DIR = os.path.join(CHROMIUM_DIR, 'third_party')
RUST_TOOLCHAIN_OUT_DIR = os.path.join(THIRD_PARTY_DIR, 'rust-toolchain')


def GetPackageVersion():
  from update import (CLANG_REVISION, CLANG_SUB_REVISION)
  return '%s-%s-%s-%s' % (RUST_REVISION, RUST_SUB_REVISION, CLANG_REVISION,
                          CLANG_SUB_REVISION)


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

  try:
    with tempfile.TemporaryFile() as f:
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


if __name__ == '__main__':
  sys.exit(main())

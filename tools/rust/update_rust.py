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

from pathlib import Path

# Add Clang scripts to path so we can import them later (if running within a
# Chromium checkout.)
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'clang',
                 'scripts'))

# Trunk on 4/1/2022
RUST_REVISION = '79f178b7'
RUST_SUB_REVISION = 1

# Hash of src/stage0.json, which itself contains the stage0 toolchain hashes.
# We trust the Rust build system checks, but to ensure it is not tampered with
# itself check the hash.
STAGE0_JSON_SHA256 = (
    'a38b7ea8b8cbdb592b1a7ae8b97fa31746a2bda309597de111be4893a035070d')


def GetPackageVersion():
  from update import (CLANG_REVISION, CLANG_SUB_REVISION)
  return '%s-%s-%s-%s' % (RUST_REVISION, RUST_SUB_REVISION, CLANG_REVISION,
                          CLANG_SUB_REVISION)


def main():
  parser = argparse.ArgumentParser(description='Update Rust package')
  parser.add_argument('--print-rust-revision',
                      action='store_true',
                      help='Print Rust revision (without Clang revision). Can '
                      'be run outside of a Chromium checkout.')
  parser.add_argument('--print-package-version',
                      action='store_true',
                      help='Print Rust package version (including both the '
                      'Rust and Clang revisions)')
  args = parser.parse_args()

  if args.print_rust_revision:
    print(f'{RUST_REVISION}-{RUST_SUB_REVISION}')
    return 0

  if args.print_package_version:
    print(GetPackageVersion())
    return 0


if __name__ == '__main__':
  sys.exit(main())

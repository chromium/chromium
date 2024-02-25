#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Run `cargo vet` against `third_party/rust/chromium_crates_io`.

Arguments are passed through to `cargo vet`.
'''

import argparse
import os
import platform
import subprocess
import sys

from run_cargo import (RunCargo, DEFAULT_SYSROOT)

_THIS_DIR = os.path.realpath(os.path.dirname(__file__))
_CHROMIUM_ROOT_DIR = os.path.join(_THIS_DIR, '..', '..')
_MANIFEST_DIR = os.path.join(_CHROMIUM_ROOT_DIR, 'third_party', 'rust',
                             'chromium_crates_io')


def main():
    parser = argparse.ArgumentParser(
        description=
        'run `cargo vet` against `//third_party/rust/chromium_crates_io`')
    parser.add_argument('--rust-sysroot',
                        default=DEFAULT_SYSROOT,
                        help='use cargo and rustc from here')
    parser.add_argument('--out-dir',
                        default='out/gnrt',
                        help='put target and cargo home dir here')
    (args, unrecognized_args) = parser.parse_known_args()

    target_dir = os.path.abspath(os.path.join(args.out_dir, 'target'))
    home_dir = os.path.abspath(os.path.join(target_dir, 'cargo_home'))

    _CARGO_ARGS = ['-Zunstable-options', '-C', _MANIFEST_DIR]
    _EXTRA_VET_ARGS = ['--cargo-arg=-Zbindeps', '--no-registry-suggestions']
    return RunCargo(args.rust_sysroot, home_dir,
                    _CARGO_ARGS + ['vet'] + unrecognized_args + _EXTRA_VET_ARGS)


if __name__ == '__main__':
    sys.exit(main())

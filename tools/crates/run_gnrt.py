#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Build and run gnrt.

Should be run from the checkout root (i.e. as `tools/crates/run_gnrt.py ...`)
'''

import argparse
import os
import platform
import subprocess
import sys

GNRT_DIR = os.path.join(os.path.abspath(os.path.dirname(__file__)), 'gnrt')
GNRT_MANIFEST_PATH = os.path.join(GNRT_DIR, 'Cargo.toml')

from run_cargo import (RunCargo, DEFAULT_SYSROOT)

def main():
    parser = argparse.ArgumentParser(description='build and run gnrt')
    parser.add_argument('--rust-sysroot',
                        default=DEFAULT_SYSROOT,
                        help='use cargo and rustc from here')
    parser.add_argument('--out-dir',
                        default='out/gnrt',
                        help='put target and cargo home dir here')
    (args, gnrt_args) = parser.parse_known_args()

    target_dir = os.path.abspath(os.path.join(args.out_dir, 'target'))
    home_dir = os.path.abspath(os.path.join(target_dir, 'cargo_home'))

    cargo_args = [
        '--locked', 'run', '--release', '--manifest-path', GNRT_MANIFEST_PATH,
        '--target-dir', target_dir, '--'
    ] + gnrt_args
    success = RunCargo(args.rust_sysroot, home_dir, cargo_args)
    return 0 if success else 1


if __name__ == '__main__':
    sys.exit(main())

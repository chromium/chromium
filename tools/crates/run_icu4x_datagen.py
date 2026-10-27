#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Build and run icu4x_datagen.

Should be run from the checkout root (i.e. as
`tools/crates/run_icu4x_datagen.py ...`).
'''

import argparse
import os
import pathlib
import sys

DATAGEN_DIR = os.path.join(
    os.path.abspath(os.path.dirname(__file__)), 'icu4x_datagen'
)
DATAGEN_MANIFEST_PATH = os.path.join(DATAGEN_DIR, 'Cargo.toml')

from run_cargo import RunCargo, DEFAULT_SYSROOT


def RunDatagen(rust_sysroot, out_dir, locked, datagen_args):
    target_dir = os.path.abspath(os.path.join(out_dir, 'target'))
    home_dir = os.path.abspath(os.path.join(target_dir, 'cargo_home'))
    source_cache_dir = os.path.abspath(os.path.join(target_dir, 'source_cache'))
    os.environ.setdefault('ICU4X_SOURCE_CACHE', source_cache_dir)
    subcommand = 'run'
    if datagen_args and datagen_args[0] == 'test':
        subcommand = 'test'
        datagen_args = datagen_args[1:]
    cargo_args = ['--locked'] if locked else []
    cargo_args += [
        subcommand,
        '--release',
        '--manifest-path',
        DATAGEN_MANIFEST_PATH,
        '--target-dir',
        target_dir,
        '--',
    ] + datagen_args
    return RunCargo(rust_sysroot, home_dir, cargo_args)


def main():
    parser = argparse.ArgumentParser(description='build and run icu4x_datagen')
    parser.add_argument(
        '--rust-sysroot',
        default=DEFAULT_SYSROOT,
        type=pathlib.Path,
        help='use cargo and rustc from here',
    )
    parser.add_argument(
        '--out-dir',
        default='out/icu4x_datagen',
        help='put cargo target and cargo home dir here',
    )
    parser.add_argument(
        '--unlocked',
        action='store_true',
        help='run cargo without --locked (to generate/update Cargo.lock)',
    )
    (args, datagen_args) = parser.parse_known_args()
    locked = not args.unlocked and os.path.exists(
        os.path.join(DATAGEN_DIR, 'Cargo.lock')
    )
    return RunDatagen(args.rust_sysroot, args.out_dir, locked, datagen_args)


if __name__ == '__main__':
    sys.exit(main())

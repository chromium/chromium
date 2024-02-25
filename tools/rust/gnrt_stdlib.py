#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Builds the gnrt tool and runs it to generate stdlib GN rules.
'''

import argparse
import os
import sys
import tempfile

# Get variables and helpers from Clang update script.
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'clang',
                 'scripts'))
from build import (RunCommand)
from update import (CHROMIUM_DIR)

sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'crates'))
from run_cargo import RunCargo

from build_bindgen import (EXE, RUST_BETA_SYSROOT_DIR, InstallRustBetaSysroot)
from build_rust import (RustTargetTriple, RUST_SRC_DIR)
from update_rust import (RUST_REVISION)

BUILD_RUST_PY_PATH = os.path.join(CHROMIUM_DIR, 'tools', 'rust',
                                  'build_rust.py')
GNRT_CARGO_TOML_PATH = os.path.join(CHROMIUM_DIR, 'tools', 'crates', 'gnrt',
                                    'Cargo.toml')


def main():
    parser = argparse.ArgumentParser(
        description='build and run gnrt for stdlib')
    parser.add_argument(
        '--skip-prep',
        action='store_true',
        help='do not fetch dependencies, assuming a previous successful run.')
    parser.add_argument(
        '--out-dir',
        help='cache artifacts in specified directory instead of a temp dir.')
    args = parser.parse_args()

    if not args.skip_prep:
        # This step gets the dependencies in the Rust git checkout.
        RunCommand([BUILD_RUST_PY_PATH, '--sync-for-gnrt'])

        # Get a Rust sysroot to build gnrt with.
        InstallRustBetaSysroot(RUST_REVISION, [RustTargetTriple()])

    cargo_bin = os.path.join(RUST_BETA_SYSROOT_DIR, 'bin', f'cargo{EXE}')
    rustc_bin = os.path.join(RUST_BETA_SYSROOT_DIR, 'bin', f'rustc{EXE}')

    # Build and run gnrt to update the stdlib GN rules.
    if args.out_dir:
        run_gnrt(RUST_BETA_SYSROOT_DIR, args.out_dir)
    else:
        with tempfile.TemporaryDirectory() as out_dir:
            run_gnrt(RUST_BETA_SYSROOT_DIR, out_dir)


def run_gnrt(sysroot_dir, out_dir):
    target_dir = os.path.abspath(os.path.join(out_dir, 'target'))
    home_dir = os.path.join(target_dir, 'cargo_home')
    RunCargo(sysroot_dir, home_dir, [
        '--quiet', '--locked', 'run', '--release', '--manifest-path',
        GNRT_CARGO_TOML_PATH, f'--target-dir={target_dir}', '--', 'gen',
        f'--for-std={os.path.relpath(RUST_SRC_DIR, CHROMIUM_DIR)}'
    ])


if __name__ == '__main__':
    sys.exit(main())

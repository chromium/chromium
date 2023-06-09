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

from build_bindgen import (EXE, InstallRustBetaSysroot)
from build_rust import (RustTargetTriple)
from update_rust import (RUST_REVISION)

BUILD_RUST_PY_PATH = os.path.join(CHROMIUM_DIR, 'tools', 'rust',
                                  'build_rust.py')
GNRT_CARGO_TOML_PATH = os.path.join(CHROMIUM_DIR, 'tools', 'crates', 'gnrt',
                                    'Cargo.toml')


def main():
    parser = argparse.ArgumentParser(
        description='build and run gnrt for stdlib')
    parser.add_argument('--rust-src-dir',
                        type=str,
                        required=True,
                        metavar='RUST_SRC_DIR',
                        help='path to the root of the Rust src tree')
    args = parser.parse_args()

    # This step gets the dependencies in the Rust git checkout.
    RunCommand([BUILD_RUST_PY_PATH, '--update-deps'])

    # Get a Rust sysroot to build gnrt with.
    root = InstallRustBetaSysroot(RUST_REVISION, [RustTargetTriple(None)])
    cargo_bin = os.path.join(root, 'bin', f'cargo{EXE}')
    rustc_bin = os.path.join(root, 'bin', f'rustc{EXE}')

    # Build and run gnrt to update the stdlib GN rules.
    with tempfile.TemporaryDirectory() as cargo_work_dir:
        cargo_env = os.environ
        cargo_env['CARGO_HOME'] = cargo_work_dir
        RunCommand([
            cargo_bin, '--quiet', '--locked', 'build', '--release',
            '--manifest-path', GNRT_CARGO_TOML_PATH,
            f'--target-dir={cargo_work_dir}', '--config',
            f'build.rustc="{rustc_bin}"'
        ])
        RunCommand([
            os.path.join(cargo_work_dir, 'release', f'gnrt{EXE}'), 'gen',
            f'--for-std={os.path.relpath(args.rust_src_dir, CHROMIUM_DIR)}',
            f'--cargo-path={cargo_bin}', f'--rustc-path={rustc_bin}'
        ])


if __name__ == '__main__':
    sys.exit(main())

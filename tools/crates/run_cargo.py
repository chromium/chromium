#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Run cargo from the chromium Rust toolchain.

Arguments are passed through to cargo.

Should be run from the checkout root (i.e. as `tools/crates/run_cargo.py ...`)
'''

import argparse
import os
import platform
import subprocess
import sys

DEFAULT_SYSROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..',
                               '..', 'third_party', 'rust-toolchain')


def RunCargo(rust_sysroot, home_dir, cargo_args):
    if not os.path.exists(rust_sysroot):
        print(f'WARNING: Rust sysroot missing at "{rust_sysroot}"')

    abs_rust_sysroot = os.path.abspath(rust_sysroot)
    bin_dir = os.path.join(abs_rust_sysroot, 'bin')

    cargo_env = dict(os.environ)
    if home_dir:
        cargo_env['CARGO_HOME'] = home_dir
    cargo_env['PATH'] = (f'{bin_dir}{os.pathsep}{cargo_env["PATH"]}'
                         if cargo_env["PATH"] else f'{bin_dir}')

    return subprocess.run([
        'cargo',
    ] + cargo_args, env=cargo_env).returncode


def main():
    parser = argparse.ArgumentParser(description='run cargo')
    parser.add_argument('--rust-sysroot',
                        default=DEFAULT_SYSROOT,
                        help='use cargo and rustc from here')
    (args, cargo_args) = parser.parse_known_args()
    return RunCargo(args.rust_sysroot, None, cargo_args)


if __name__ == '__main__':
    sys.exit(main())

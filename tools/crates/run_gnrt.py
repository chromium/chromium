#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Build and run gnrt.

Should be run from the checkout root (i.e. as `tools/crates/run_gnrt.py ...`)
'''

import argparse
import os
import subprocess
import sys

GNRT_DIR = os.path.join(os.path.abspath(os.path.dirname(__file__)), 'gnrt')
GNRT_MANIFEST_PATH = os.path.join(GNRT_DIR, 'Cargo.toml')


def main():
    parser = argparse.ArgumentParser(description='build and run gnrt')
    parser.add_argument('--rust-sysroot',
                        default='third_party/rust-toolchain',
                        help='use cargo and rustc from here')
    parser.add_argument('--out-dir',
                        default='out/gnrt',
                        help='put target and cargo home dir here')
    parser.add_argument('gnrt_args',
                        nargs='*',
                        help='additional arguments to pass to gnrt, e.g. "gen"')
    args = parser.parse_args()

    exe = ''
    if sys.platform == 'win32':
        exe = '.exe'

    cargo_bin = os.path.abspath(
        os.path.join(args.rust_sysroot, 'bin', f'cargo{exe}'))
    rustc_bin = os.path.abspath(
        os.path.join(args.rust_sysroot, 'bin', f'rustc{exe}'))

    cargo_env = os.environ
    cargo_env['CARGO_HOME'] = os.path.abspath(
        os.path.join(args.out_dir, 'cargo_home'))
    target_dir = os.path.abspath(os.path.join(args.out_dir, 'target'))

    gnrt_args = args.gnrt_args
    if gnrt_args and gnrt_args[0] == 'gen':
        gnrt_args.extend(['--rustc-path', rustc_bin])
    return subprocess.run([
        cargo_bin, '--locked', 'run', '--release', '--manifest-path',
        GNRT_MANIFEST_PATH, '--target-dir', target_dir, '--config',
        f'build.rustc="{rustc_bin}"', '--'
    ] + gnrt_args).returncode


if __name__ == '__main__':
    sys.exit(main())

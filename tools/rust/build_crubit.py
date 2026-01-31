#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Builds the Crubit tools.

Builds the Crubit tools for generating Rust/C++ FFI bindings.

This script must be run after //tools/rust/build_rust.py as it uses the outputs
of that script in the compilation of Crubit. In particular it uses:
- The rust toolchain binaries and libraries in `RUST_TOOLCHAIN_OUT_DIR`.
- In the future (if/when building `rs_bindings_from_cc`) it may also use:
  The LLVM and Clang libraries and headers in `RUST_HOST_LLVM_INSTALL_DIR`.

This script:
- Clones the Crubit repository, checks out a defined revision.
- Builds Crubit's `cc_bindings_from_rs` using Cargo.
- Copies cc_bindings_from_rs into `RUST_TOOLCHAIN_OUT_DIR`.

The `rs_bindings_from_cs` binary is not yet built,
as Cargo builds of `rs_bindings_from_cs` are not yet
officially supported by the Crubit team.
'''

import argparse
import os
import platform
import shutil
import sys
import tempfile

from pathlib import Path

# Get variables and helpers from Clang update script
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'clang',
                 'scripts'))

from build import (AddCMakeToPath, AddZlibToPath, CheckoutGitRepo,
                   DownloadDebianSysroot, RunCommand, THIRD_PARTY_DIR)
from update import (RmTree)

from build_rust import (RUST_HOST_LLVM_INSTALL_DIR)
from update_rust import (CHROMIUM_DIR, CRUBIT_REVISION, RUST_TOOLCHAIN_OUT_DIR)

# Get `RunCargo` from `//tools/crates/run_cargo.py`.
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'crates'))
from run_cargo import RunCargo

CRUBIT_GIT = 'https://github.com/google/crubit'

CRUBIT_SRC_DIR = os.path.join(CHROMIUM_DIR, 'third_party',
                              'rust-toolchain-intermediate', 'crubit')
CC_BINDINGS_FROM_RS_CARGO_TOML_PATH = os.path.join(CRUBIT_SRC_DIR, "cargo",
                                                   "cc_bindings_from_rs",
                                                   "cc_bindings_from_rs",
                                                   "Cargo.toml")

EXE = '.exe' if sys.platform == 'win32' else ''


def GetCcBindingsFromRsRustFlags():
    # Need to help the runtime linker find the path to
    # `librustc_driver-xxxxxxxxxxxxxxxx.so`.  This mimics how `rustc` is built
    # as seen in
    # https://github.com/rust-lang/rust/blob/b889870082dd0b0e3594bbfbebb4545d54710829/src/bootstrap/src/core/builder/cargo.rs#L285-L306
    # See also https://crbug.com/460482110#comment14 - #comment16
    if sys.platform == 'darwin':
        return [
            "-Zosx-rpath-install-name",
            "-Clink-args=-Wl,-rpath,@loader_path/../lib"
        ]
    elif sys.platform != 'win32':
        return [
            "-Clink-args=-Wl,-z,origin",
            "-Clink-args=-Wl,-rpath,$ORIGIN/../lib"
        ]
    else:
        return []


def BuildCrubit(rust_sysroot, out_dir):
    target_dir = os.path.abspath(os.path.join(out_dir, 'target'))
    release_dir = os.path.join(target_dir, 'release')
    home_dir = os.path.join(target_dir, 'cargo_home')

    print(f'Building cc_bindings_from_rs...')
    cargo_args = ['build', '--release', '--verbose']
    cargo_args += ['--bin', 'cc_bindings_from_rs']
    cargo_args += ['--target-dir', target_dir]
    cargo_args += ['--manifest-path', CC_BINDINGS_FROM_RS_CARGO_TOML_PATH]
    extra_rustflags = GetCcBindingsFromRsRustFlags()
    cargo_result = RunCargo(rust_sysroot, home_dir, cargo_args,
                            extra_rustflags)
    if cargo_result:
        return cargo_result

    print(f'Installing Crubit to {RUST_TOOLCHAIN_OUT_DIR} ...')
    CRUBIT_BINS = ['cc_bindings_from_rs']
    for bin in CRUBIT_BINS:
        bin = bin + EXE
        shutil.copy(os.path.join(release_dir, bin),
                    os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'bin', bin))
    return 0


def main():
    parser = argparse.ArgumentParser(
        description='Build and package Crubit tools')
    parser.add_argument(
        '--skip-checkout',
        action='store_true',
        help=('skip checking out source code. Useful for trying local'
              'changes'))
    parser.add_argument(
        '--out-dir',
        help='cache artifacts in specified directory instead of a temp dir.')
    parser.add_argument('--debug',
                        action='store_true',
                        help=('build Crubit in debug mode'))
    args = parser.parse_args()

    if not args.skip_checkout:
        CheckoutGitRepo("crubit", CRUBIT_GIT, CRUBIT_REVISION, CRUBIT_SRC_DIR)

    if args.out_dir:
        return BuildCrubit(RUST_TOOLCHAIN_OUT_DIR, args.out_dir)
    else:
        with tempfile.TemporaryDirectory() as out_dir:
            return BuildCrubit(RUST_TOOLCHAIN_OUT_DIR, out_dir)


if __name__ == '__main__':
    sys.exit(main())

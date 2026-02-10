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
import json
import os
import platform
import shutil
import sys
import tempfile
import urllib

from pathlib import Path

# Get variables and helpers from `//tools/clang/scripts/build.py`.
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'clang',
                 'scripts'))
from build import (AddZlibToPath, GetLibXml2Dirs, CheckoutGitRepo)

from update_rust import (CHROMIUM_DIR, CRUBIT_REVISION, RUST_TOOLCHAIN_OUT_DIR)

# Get `RunCargo` from `//tools/crates/run_cargo.py`.
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'crates'))
from run_cargo import RunCargo

CRUBIT_GIT = 'https://chromium.googlesource.com/external/github.com/google/crubit'

CRUBIT_SRC_DIR = os.path.join(CHROMIUM_DIR, 'third_party',
                              'rust-toolchain-intermediate', 'crubit')
CC_BINDINGS_FROM_RS_CARGO_TOML_PATH = os.path.join(CRUBIT_SRC_DIR, "cargo",
                                                   "cc_bindings_from_rs",
                                                   "cc_bindings_from_rs",
                                                   "Cargo.toml")

EXE = '.exe' if sys.platform == 'win32' else ''


def GetLatestCrubitCommit():
    """Get the latest commit hash in the Crubit repo."""
    url = CRUBIT_GIT + '/+/refs/heads/upstream/main?format=JSON'
    main = json.loads(
        urllib.request.urlopen(url).read().decode("utf-8").replace(")]}'", ""))
    return main['commit']


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


def GetNativeLibsRustFlags():
    if sys.platform == 'win32':
        # See https://crbug.com/481661885 to learn why adding `zlib.lib` and
        # `libxml2s.lib` paths is required to build `cc_bindings_from_rs` on
        # Windows when using Chromium-built Rust sysroot.
        #
        # Note that some of the calls below may be expensive (e.g. downloading
        # zlib sources and building it) so `GetNativeLibsRustFlags` probably
        # shouldn't be called in incremental builds (e.g. when
        # `--skip-checkout` is present).
        libxml2_lib_path = GetLibXml2Dirs().lib_dir
        zlib_lib_path = AddZlibToPath()
        return [
            f'-Clink-arg=/LIBPATH:{libxml2_lib_path}',
            f'-Clink-arg=/LIBPATH:{zlib_lib_path}'
        ]

    # No native libs needed on other platforms:
    return []


def BuildCrubit(rust_sysroot, out_dir, skip_checkout):
    target_dir = os.path.abspath(os.path.join(out_dir, 'target'))
    release_dir = os.path.join(target_dir, 'release')
    home_dir = os.path.join(target_dir, 'cargo_home')

    print(f'Building cc_bindings_from_rs ...')
    cargo_args = ['build', '--release', '--verbose']
    cargo_args += ['--bin', 'cc_bindings_from_rs']
    cargo_args += ['--target-dir', target_dir]
    cargo_args += ['--manifest-path', CC_BINDINGS_FROM_RS_CARGO_TOML_PATH]
    extra_rustflags = GetCcBindingsFromRsRustFlags()
    if not skip_checkout:
        extra_rustflags += GetNativeLibsRustFlags()
    cargo_result = RunCargo(rust_sysroot, home_dir, cargo_args,
                            extra_rustflags)
    print(f'Building cc_bindings_from_rs ... done.  Result: {cargo_result}')
    if cargo_result:
        return cargo_result

    print(f'Installing Crubit to {RUST_TOOLCHAIN_OUT_DIR} ...')
    CRUBIT_BINS = ['cc_bindings_from_rs']
    for bin in CRUBIT_BINS:
        bin = bin + EXE
        print(f'    Copying {bin} ...')
        shutil.copy(os.path.join(release_dir, bin),
                    os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'bin', bin))

    print(f'Installing `crubit/support` to {RUST_TOOLCHAIN_OUT_DIR} ...')
    crubit_support_install_dir = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'lib',
                                              'crubit', 'support')
    os.makedirs(crubit_support_install_dir, exist_ok=True)
    crubit_support_src_dir = os.path.join(CRUBIT_SRC_DIR, 'support')
    shutil.copytree(crubit_support_src_dir,
                    crubit_support_install_dir,
                    dirs_exist_ok=True)

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
    parser.add_argument('--crubit-force-head-revision',
                        action='store_true',
                        help=('build the most recent commit of crubit '
                              'instead of the current pinned version'))
    args = parser.parse_args()

    if args.crubit_force_head_revision:
        crubit_revision = GetLatestCrubitCommit()
    else:
        crubit_revision = CRUBIT_REVISION

    if not args.skip_checkout:
        CheckoutGitRepo("crubit", CRUBIT_GIT, crubit_revision, CRUBIT_SRC_DIR)

    if args.out_dir:
        return BuildCrubit(RUST_TOOLCHAIN_OUT_DIR, args.out_dir,
                           args.skip_checkout)
    else:
        with tempfile.TemporaryDirectory() as out_dir:
            return BuildCrubit(RUST_TOOLCHAIN_OUT_DIR, out_dir,
                               args.skip_checkout)


if __name__ == '__main__':
    sys.exit(main())

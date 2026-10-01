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
import contextlib
import os
import shutil
import subprocess
import sys
import tempfile

# Get variables and helpers from `//tools/clang/scripts/build.py`.
sys.path.append(
    os.path.join(
        os.path.dirname(os.path.abspath(__file__)), '..', 'clang', 'scripts'
    )
)
from build import (
    AddZlibToPath,
    CheckoutGitRepo,
    GetLatestCommit,
    GetLibXml2Dirs,
)

from host_tools import EXE, GetHostCargoEnv, RunHostCargo
from update_rust import CHROMIUM_DIR, CRUBIT_REVISION, RUST_TOOLCHAIN_OUT_DIR

CRUBIT_GIT = (
    'https://chromium.googlesource.com/external/github.com/google/crubit'
)

CRUBIT_SRC_DIR = os.path.join(
    CHROMIUM_DIR, 'third_party', 'rust-toolchain-intermediate', 'crubit'
)

# The Crubit binaries that this script builds and installs.  All of them are
# members of the root cargo workspace (see Crubit's `Cargo.toml`).
CRUBIT_BINS = ['cc_bindings_from_rs']

# One argument per binary that makes it start, print a short text, and exit
# with 0 (see `SmokeTestCrubit`).
SMOKE_TEST_ARGS = {
    'cc_bindings_from_rs': '--help',
}

IS_WIN = sys.platform == 'win32'
IS_MAC = sys.platform == 'darwin'


def GetLatestCrubitCommit():
    """Get the latest commit hash in the Crubit repo."""
    url = CRUBIT_GIT + '/+/refs/heads/upstream/main?format=JSON'
    return GetLatestCommit(url)


def GetRustcDriverRpathFlags():
    """Returns rustflags that help `cc_bindings_from_rs` find `rustc_driver`.

    We need to help the runtime linker find the path to
    `librustc_driver-xxxxxxxxxxxxxxxx.so`.  This mimics how `rustc` is built
    as seen in
    https://github.com/rust-lang/rust/blob/b889870082dd0b0e3594bbfbebb4545d54710829/src/bootstrap/src/core/builder/cargo.rs#L285-L306
    See also https://crbug.com/460482110#comment14 - #comment16
    """
    if IS_WIN:
        return []
    if IS_MAC:
        return [
            '-Zosx-rpath-install-name',
            '-Clink-args=-Wl,-rpath,@loader_path/../lib',
        ]
    return [
        '-Clink-args=-Wl,-z,origin',
        '-Clink-args=-Wl,-rpath,$ORIGIN/../lib',
    ]


def GetNativeLibsRustFlags():
    """Returns rustflags needed to link native libs on Windows.

    See https://crbug.com/481661885 to learn why adding `zlib.lib` and
    `libxml2s.lib` paths is required to build `cc_bindings_from_rs` on Windows
    when using a Chromium-built Rust sysroot.

    Both libraries are built by the prerequisite `build_rust.py` run (zlib
    directly, libxml2 as part of the LLVM build), so we only compute their
    paths here.  In particular `AddZlibToPath(dry_run=False)` must not be used:
    it deletes and rebuilds zlib, and leaves the process CWD inside `zlib_dir`.
    """
    if not IS_WIN:
        # No native libs needed on other platforms:
        return []

    libxml2_lib_dir = GetLibXml2Dirs().lib_dir
    zlib_lib_dir = AddZlibToPath(dry_run=True)
    # Neither `dry_run=True` nor `GetLibXml2Dirs` checks that the libraries are
    # really there, so verify here - otherwise a missing prerequisite shows up
    # much later as an obscure linker error.
    for lib in [
        os.path.join(zlib_lib_dir, 'zlib.lib'),
        os.path.join(libxml2_lib_dir, 'libxml2s.lib'),
    ]:
        if not os.path.exists(lib):
            raise RuntimeError(
                f'{lib} not found.  Run `tools/rust/build_rust.py` first.'
            )

    return [
        f'-Clink-arg=/LIBPATH:{libxml2_lib_dir}',
        f'-Clink-arg=/LIBPATH:{zlib_lib_dir}',
    ]


def GetCrubitRustFlags():
    """Returns the Crubit-specific rustflags.

    `BuildCrubitBinaries` adds them to the rustflags that `GetHostCargoEnv`
    uses for all host tools.
    """
    return GetRustcDriverRpathFlags() + GetNativeLibsRustFlags()


def BuildCrubitBinaries(target_dir, home_dir):
    """Builds all of `CRUBIT_BINS`; exits if `cargo` fails."""
    # All of `CRUBIT_BINS` are members of the root cargo workspace, so one
    # `cargo build` against the workspace manifest builds all of them.
    bins = ' and '.join(CRUBIT_BINS)
    print(f'Building {bins} ...')
    cargo_args = ['build', '--release', '--verbose']
    # Crubit checks in a `Cargo.lock`; build exactly the versions it pins.
    cargo_args += ['--locked']
    # `-p` stops cargo from enabling dependency features that only other
    # workspace members need.  Each binary has a package with the same name.
    for bin_name in CRUBIT_BINS:
        cargo_args += ['-p', bin_name, '--bin', bin_name]
    cargo_args += ['--target-dir', target_dir]
    workspace_cargo_toml = os.path.join(CRUBIT_SRC_DIR, 'Cargo.toml')
    cargo_args += ['--manifest-path', workspace_cargo_toml]
    env = GetHostCargoEnv(cargo_home=home_dir)
    env['RUSTFLAGS'] += ' ' + ' '.join(GetCrubitRustFlags())
    RunHostCargo(cargo_args, env)
    print(f'Building {bins} ... done.')


def InstallCrubit(release_dir):
    """Copies the built binaries and Crubit's support library into place."""
    print(f'Installing Crubit to {RUST_TOOLCHAIN_OUT_DIR} ...')
    for bin_name in CRUBIT_BINS:
        bin_exe = bin_name + EXE
        print(f'    Copying {bin_exe} ...')
        shutil.copy(
            os.path.join(release_dir, bin_exe),
            os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'bin', bin_exe),
        )

    # `crubit_target_dir` below helps ensure that Chromium can use the same
    # `#include` paths as other Crubit clients like google3 - e.g.
    # `#include "third_party/crubit/support/rs_std/slice_ref.h"`.
    print(f'Installing `crubit/support` to {RUST_TOOLCHAIN_OUT_DIR} ...')
    crubit_target_dir = os.path.join(
        RUST_TOOLCHAIN_OUT_DIR, 'lib', 'third_party', 'crubit'
    )
    for item in ['BUILD.gn', 'LICENSE', 'crubit.gni', 'support']:
        source_path = os.path.join(CRUBIT_SRC_DIR, item)
        target_path = os.path.join(crubit_target_dir, item)
        os.makedirs(os.path.dirname(target_path), exist_ok=True)
        if os.path.isdir(source_path):
            shutil.copytree(source_path, target_path, dirs_exist_ok=True)
        else:
            shutil.copy2(source_path, target_path)


def SmokeTestCrubit():
    """Runs each installed binary once, to find loader and start-up errors."""
    for bin_name in CRUBIT_BINS:
        exe = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'bin', bin_name + EXE)
        # Keep stderr: loader errors go there.
        subprocess.run(
            [exe, SMOKE_TEST_ARGS[bin_name]],
            stdout=subprocess.DEVNULL,
            check=True,
        )


def BuildCrubit(out_dir):
    target_dir = os.path.abspath(os.path.join(out_dir, 'target'))
    release_dir = os.path.join(target_dir, 'release')
    home_dir = os.path.join(target_dir, 'cargo_home')

    BuildCrubitBinaries(target_dir, home_dir)
    InstallCrubit(release_dir)
    SmokeTestCrubit()


def main():
    parser = argparse.ArgumentParser(
        description='Build and package Crubit tools'
    )
    parser.add_argument(
        '--skip-checkout',
        action='store_true',
        help=('skip checking out source code. Useful for trying localchanges'),
    )
    parser.add_argument(
        '--out-dir',
        help='cache artifacts in specified directory instead of a temp dir.',
    )
    parser.add_argument(
        '--crubit-force-head-revision',
        action='store_true',
        help=(
            'build the most recent commit of crubit '
            'instead of the current pinned version'
        ),
    )
    args = parser.parse_args()

    if args.crubit_force_head_revision:
        crubit_revision = GetLatestCrubitCommit()
    else:
        crubit_revision = CRUBIT_REVISION

    if not args.skip_checkout:
        CheckoutGitRepo('crubit', CRUBIT_GIT, crubit_revision, CRUBIT_SRC_DIR)

    with contextlib.ExitStack() as stack:
        out_dir = args.out_dir or stack.enter_context(
            tempfile.TemporaryDirectory()
        )
        BuildCrubit(out_dir)
    return 0


if __name__ == '__main__':
    sys.exit(main())

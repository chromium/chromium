#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Builds the bindgen tool.'''

import argparse
import os
from pathlib import Path
import platform
import shutil
import sys

from build_rust import (
    CIPD_DOWNLOAD_URL,
    FetchBetaPackage,
    InstallBetaPackage,
    RustTargetTriple,
    RUST_HOST_LLVM_INSTALL_DIR,
)
from host_tools import (
    EXE,
    GetHostCargoEnv,
    RunHostCargo,
)

from update_rust import (
    RUST_TOOLCHAIN_OUT_DIR,
    THIRD_PARTY_DIR,
    BINDGEN_REVISION,
)

# Get variables and helpers from Clang update script
sys.path.append(
    os.path.join(
        os.path.dirname(os.path.abspath(__file__)), '..', 'clang', 'scripts'
    )
)

from build import (
    CheckoutGitRepo,
    DownloadAndUnpack,
    LLVM_BUILD_TOOLS_DIR,
)
from update import RmTree

BINDGEN_GIT_VERSION = BINDGEN_REVISION
BINDGEN_GIT_REPO = (
    'https://chromium.googlesource.com/external/'
    + 'github.com/rust-lang/rust-bindgen'
)

BINDGEN_SRC_DIR = os.path.join(
    THIRD_PARTY_DIR, 'rust-toolchain-intermediate', 'bindgen-src'
)
BINDGEN_HOST_BUILD_DIR = os.path.join(
    THIRD_PARTY_DIR, 'rust-toolchain-intermediate', 'bindgen-host-build'
)
BINDGEN_CROSS_TARGET_BUILD_DIR = os.path.join(
    THIRD_PARTY_DIR, 'rust-toolchain-intermediate', 'bindgen-target-build'
)

NCURSESW_CIPD_LINUX_AMD_PATH = 'infra/3pp/static_libs/ncursesw/linux-amd64'
NCURSESW_CIPD_LINUX_AMD_VERSION = '6.0.chromium.1'

# TODO(crbug.com/558838938) Not all tests pass.
EXCLUDED_TESTS = [
    'header_issue_753_h',
    'header_macro_fallback_include_builtin_h',
]


def FetchNcurseswLibrary():
    assert sys.platform.startswith('linux')
    ncursesw_dir = os.path.join(LLVM_BUILD_TOOLS_DIR, 'ncursesw')
    ncursesw_url = (
        f'{CIPD_DOWNLOAD_URL}/{NCURSESW_CIPD_LINUX_AMD_PATH}'
        f'/+/version:2@{NCURSESW_CIPD_LINUX_AMD_VERSION}'
    )

    if os.path.exists(ncursesw_dir):
        RmTree(ncursesw_dir)
    DownloadAndUnpack(ncursesw_url, ncursesw_dir, is_known_zip=True)
    return ncursesw_dir


def RunCargo(cargo_args):
    """Runs `cargo` (see `host_tools.py`) with the settings that bindgen needs.

    This will `fail_hard` and not return if `cargo` reports problems.
    """
    ncursesw_dir = None
    if sys.platform.startswith('linux'):
        ncursesw_dir = FetchNcurseswLibrary()

    llvm_dir = RUST_HOST_LLVM_INSTALL_DIR

    if not os.path.exists(os.path.join(llvm_dir, 'bin', f'llvm-config{EXE}')):
        print(
            f'Missing llvm-config in {llvm_dir}. This '
            f'script expects to be run after build_rust.py is run as '
            f'the build_rust.py script produces the LLVM libraries that '
            f'are needed here.'
        )
        sys.exit(1)

    env = GetHostCargoEnv()

    # Use the LLVM libs from the rustc build.
    env['LLVM_CONFIG_PATH'] = os.path.join(llvm_dir, 'bin', 'llvm-config')
    if sys.platform == 'win32':
        env['LIBCLANG_PATH'] = os.path.join(llvm_dir, 'bin')
    else:
        env['LIBCLANG_PATH'] = os.path.join(llvm_dir, 'lib')
    env['LIBCLANG_STATIC_PATH'] = os.path.join(llvm_dir, 'lib')

    if ncursesw_dir:
        env['CFLAGS'] += f' -I{ncursesw_dir}/include'
        env['CXXFLAGS'] += f' -I{ncursesw_dir}/include'
        env['LDFLAGS'] += f' -L{ncursesw_dir}/lib'
        env['RUSTFLAGS'] += f' -Clink-arg=-L{ncursesw_dir}/lib'

    RunHostCargo(cargo_args, env)


def main():
    parser = argparse.ArgumentParser(description='Build and package bindgen')
    parser.add_argument(
        '--skip-checkout',
        action='store_true',
        help='skip downloading the git repo. Useful for trying local changes',
    )
    parser.add_argument(
        '--skip-test', action='store_true', help='skip running tests'
    )
    args, rest = parser.parse_known_args()

    if not args.skip_checkout:
        CheckoutGitRepo(
            "bindgen", BINDGEN_GIT_REPO, BINDGEN_GIT_VERSION, BINDGEN_SRC_DIR
        )

    build_dir = BINDGEN_HOST_BUILD_DIR
    if os.path.exists(build_dir):
        RmTree(build_dir)

    print(f'Building bindgen in {build_dir} ...')
    cargo_shared_args = [
        f'--manifest-path={BINDGEN_SRC_DIR}/Cargo.toml',
        f'--target-dir={build_dir}',
    ]
    # We've run into incremental compilation bugs while building bindgen in
    # https://crbug.com/488049150, so clean the build directory first. This
    # doesn't take long compared to the rest of the build anyway.
    # `cargo clean` requires a CACHEDIR.TAG file in the directory.
    cachedir_tag = Path(build_dir) / 'CACHEDIR.TAG'
    cachedir_tag.parent.mkdir(exist_ok=True, parents=True)
    cachedir_tag.write_bytes(
        b"Signature: 8a477f597d28d172789f06886806bc55\n"
        b"# Written by build_bindgen.py to make `cargo clean` happy.\n"
    )
    RunCargo(
        [
            'clean',
        ]
        + cargo_shared_args
    )
    static_feature = ",static" if ('windows' not in RustTargetTriple()) else ""
    cargo_args = [
        'build',
        f'--target={RustTargetTriple()}',
        '--no-default-features',
        '--features=logging' + static_feature,
        '--release',
        '--bin',
        'bindgen',
    ] + cargo_shared_args
    RunCargo(cargo_args)

    install_dir = os.path.join(RUST_TOOLCHAIN_OUT_DIR)
    print(f'Installing bindgen to {install_dir} ...')

    llvm_dir = RUST_HOST_LLVM_INSTALL_DIR
    shutil.copy(
        os.path.join(build_dir, RustTargetTriple(), 'release', f'bindgen{EXE}'),
        os.path.join(install_dir, 'bin'),
    )
    if sys.platform == 'win32':
        shutil.copy(
            os.path.join(llvm_dir, 'bin', 'libclang.dll'),
            os.path.join(install_dir, 'bin'),
        )
    elif sys.platform == 'darwin':
        shutil.copy(
            os.path.join(llvm_dir, 'lib', 'libclang.dylib'),
            os.path.join(install_dir, 'lib'),
        )
    else:
        # Can't replace symlinks so remove existing ones.
        for filename in os.listdir(os.path.join(install_dir, 'lib')):
            if filename.startswith('libclang.so'):
                os.remove(os.path.join(install_dir, 'lib', filename))
        for filename in os.listdir(os.path.join(llvm_dir, 'lib')):
            if filename.startswith('libclang.so'):
                shutil.copy(
                    os.path.join(llvm_dir, 'lib', filename),
                    os.path.join(install_dir, 'lib'),
                    follow_symlinks=False,
                )

    if not args.skip_test:
        test_args = ['test', '--lib', '--bins', '--tests']
        test_args += cargo_shared_args
        test_args.append('--')
        for excluded in EXCLUDED_TESTS:
            test_args.append('--skip')
            test_args.append(excluded)
        RunCargo(test_args)

    return 0


if __name__ == '__main__':
    sys.exit(main())

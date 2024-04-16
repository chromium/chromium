#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Builds the bindgen tool.'''

import argparse
import collections
import os
import platform
import shutil
import subprocess
import sys

from build_rust import (CARGO_HOME_DIR, CIPD_DOWNLOAD_URL, FetchBetaPackage,
                        InstallBetaPackage, RustTargetTriple,
                        RUST_HOST_LLVM_INSTALL_DIR)
from update_rust import (RUST_TOOLCHAIN_OUT_DIR, THIRD_PARTY_DIR)

# Get variables and helpers from Clang update script
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'clang',
                 'scripts'))

from build import (CheckoutGitRepo, DownloadAndUnpack, LLVM_BUILD_TOOLS_DIR,
                   DownloadDebianSysroot, RunCommand)
from update import (RmTree)

# The git hash to use.
BINDGEN_GIT_VERSION = 'f93d5dfa6d5d7409bea584f3eab38e1fc52b8360'
BINDGEN_GIT_REPO = ('https://chromium.googlesource.com/external/' +
                    'github.com/rust-lang/rust-bindgen')

BINDGEN_SRC_DIR = os.path.join(THIRD_PARTY_DIR, 'rust-toolchain-intermediate',
                               'bindgen-src')
BINDGEN_HOST_BUILD_DIR = os.path.join(THIRD_PARTY_DIR,
                                      'rust-toolchain-intermediate',
                                      'bindgen-host-build')
BINDGEN_CROSS_TARGET_BUILD_DIR = os.path.join(THIRD_PARTY_DIR,
                                              'rust-toolchain-intermediate',
                                              'bindgen-target-build')

NCURSESW_CIPD_LINUX_AMD_PATH = 'infra/3pp/static_libs/ncursesw/linux-amd64'
NCURSESW_CIPD_LINUX_AMD_VERSION = '6.0.chromium.1'

RUST_BETA_SYSROOT_DIR = os.path.join(THIRD_PARTY_DIR,
                                     'rust-toolchain-intermediate',
                                     'beta-sysroot')

EXE = '.exe' if sys.platform == 'win32' else ''


def InstallRustBetaSysroot(rust_git_hash, target_triples):
    if os.path.exists(RUST_BETA_SYSROOT_DIR):
        RmTree(RUST_BETA_SYSROOT_DIR)
    InstallBetaPackage(FetchBetaPackage('cargo', rust_git_hash),
                       RUST_BETA_SYSROOT_DIR)
    InstallBetaPackage(FetchBetaPackage('rustc', rust_git_hash),
                       RUST_BETA_SYSROOT_DIR)
    for t in target_triples:
        InstallBetaPackage(
            FetchBetaPackage('rust-std', rust_git_hash, triple=t),
            RUST_BETA_SYSROOT_DIR)


def FetchNcurseswLibrary():
    assert sys.platform.startswith('linux')
    ncursesw_dir = os.path.join(LLVM_BUILD_TOOLS_DIR, 'ncursesw')
    ncursesw_url = (f'{CIPD_DOWNLOAD_URL}/{NCURSESW_CIPD_LINUX_AMD_PATH}'
                    f'/+/version:2@{NCURSESW_CIPD_LINUX_AMD_VERSION}')

    if os.path.exists(ncursesw_dir):
        RmTree(ncursesw_dir)
    DownloadAndUnpack(ncursesw_url, ncursesw_dir, is_known_zip=True)
    return ncursesw_dir


def RunCargo(cargo_args):
    """Invokes `cargo` produced by an earlier `build_rust.py` step.  Note that
    this is different from the `RunCargo` function in
    `//tools/crates/run_cargo.py` which works from within a Chromium repo, but
    wouldn't work on the toolchain bots.

    Note that some environment variables populated below are not necessary for
    all users of this function (e.g. `build_vet.py` doesn't need clang/llvm
    parts).  That's a bit icky, but ultimately okay.
    """
    ncursesw_dir = None
    if sys.platform.startswith('linux'):
        ncursesw_dir = FetchNcurseswLibrary()

    cargo_bin = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'bin', f'cargo{EXE}')
    rustc_bin = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'bin', f'rustc{EXE}')
    if not os.path.exists(cargo_bin):
        print(f'Missing cargo at {cargo_bin}. This '
              f'script expects to be run after build_rust.py is run as '
              f'the build_rust.py script builds cargo that is needed here.')
        sys.exit(1)
    if not os.path.exists(rustc_bin):
        print(f'Missing rustc at {rustc_bin}. This '
              f'script expects to be run after build_rust.py is run as '
              f'the build_rust.py script builds rustc that is needed here.')
        sys.exit(1)

    clang_bins_dir = os.path.join(RUST_HOST_LLVM_INSTALL_DIR, 'bin')
    llvm_dir = RUST_HOST_LLVM_INSTALL_DIR

    if not os.path.exists(os.path.join(llvm_dir, 'bin', f'llvm-config{EXE}')):
        print(f'Missing llvm-config in {llvm_dir}. This '
              f'script expects to be run after build_rust.py is run as '
              f'the build_rust.py script produces the LLVM libraries that '
              f'are needed here.')
        sys.exit(1)

    env = collections.defaultdict(str, os.environ)
    # Cargo normally stores files in $HOME. Override this.
    env['CARGO_HOME'] = CARGO_HOME_DIR

    # Use a rustc we deterministically provide, not a system one.
    env['RUSTC'] = rustc_bin

    # Use the LLVM libs and clang compiler from the rustc build.
    env['LLVM_CONFIG_PATH'] = os.path.join(llvm_dir, 'bin', 'llvm-config')
    if sys.platform == 'win32':
        env['LIBCLANG_PATH'] = os.path.join(llvm_dir, 'bin')
    else:
        env['LIBCLANG_PATH'] = os.path.join(llvm_dir, 'lib')
    env['LIBCLANG_STATIC_PATH'] = os.path.join(llvm_dir, 'lib')

    if sys.platform == 'win32':
        env['CC'] = os.path.join(clang_bins_dir, 'clang-cl')
        env['CXX'] = os.path.join(clang_bins_dir, 'clang-cl')
    else:
        env['CC'] = os.path.join(clang_bins_dir, 'clang')
        env['CXX'] = os.path.join(clang_bins_dir, 'clang++')

    # Windows uses lld-link for MSVC compat. Otherwise, we use lld via clang.
    if sys.platform == 'win32':
        linker = os.path.join(clang_bins_dir, 'lld-link')
    else:
        linker = os.path.join(clang_bins_dir, 'clang')
        env['LDFLAGS'] += ' -fuse-ld=lld'
        env['RUSTFLAGS'] += ' -Clink-arg=-fuse-ld=lld'
    env['LD'] = linker
    env['RUSTFLAGS'] += f' -Clinker={linker}'

    if sys.platform.startswith('linux'):
        # We use these flags to avoid linking with the system libstdc++.
        sysroot = DownloadDebianSysroot('amd64')
        sysroot_flag = f'--sysroot={sysroot}'
        env['CFLAGS'] += f' {sysroot_flag}'
        env['CXXFLAGS'] += f' {sysroot_flag}'
        env['LDFLAGS'] += f' {sysroot_flag}'
        env['RUSTFLAGS'] += f' -Clink-arg={sysroot_flag}'

    if ncursesw_dir:
        env['CFLAGS'] += f' -I{ncursesw_dir}/include'
        env['CXXFLAGS'] += f' -I{ncursesw_dir}/include'
        env['LDFLAGS'] += f' -L{ncursesw_dir}/lib'
        env['RUSTFLAGS'] += f' -Clink-arg=-L{ncursesw_dir}/lib'

    if sys.platform == 'darwin':
        # The system/xcode compiler would find system SDK correctly, but
        # the Clang we've built does not. See
        # https://github.com/llvm/llvm-project/issues/45225
        sdk_path = subprocess.check_output(['xcrun', '--show-sdk-path'],
                                           text=True).rstrip()
        env['CFLAGS'] += f' -isysroot {sdk_path}'
        env['CXXFLAGS'] += f' -isysroot {sdk_path}'
        env['LDFLAGS'] += f' -isysroot {sdk_path}'
        env['RUSTFLAGS'] += f' -Clink-arg=-isysroot -Clink-arg={sdk_path}'

    # This will `fail_hard` and not return if `cargo` reports problems.
    RunCommand([cargo_bin] + cargo_args, setenv=True, env=env)


def main():
    parser = argparse.ArgumentParser(description='Build and package bindgen')
    parser.add_argument(
        '--skip-checkout',
        action='store_true',
        help='skip downloading the git repo. Useful for trying local changes')
    args, rest = parser.parse_known_args()

    if not args.skip_checkout:
        CheckoutGitRepo("bindgen", BINDGEN_GIT_REPO, BINDGEN_GIT_VERSION,
                        BINDGEN_SRC_DIR)

    build_dir = BINDGEN_HOST_BUILD_DIR
    if os.path.exists(build_dir):
        RmTree(build_dir)

    print(f'Building bindgen in {build_dir} ...')
    cargo_args = [
        'build',
        f'--manifest-path={BINDGEN_SRC_DIR}/Cargo.toml',
        f'--target-dir={build_dir}',
        f'--target={RustTargetTriple()}',
        f'--no-default-features',
        f'--features=logging',
        '--release',
        '--bin',
        'bindgen',
    ]
    RunCargo(cargo_args)

    install_dir = os.path.join(RUST_TOOLCHAIN_OUT_DIR)
    print(f'Installing bindgen to {install_dir} ...')

    llvm_dir = RUST_HOST_LLVM_INSTALL_DIR
    shutil.copy(
        os.path.join(build_dir, RustTargetTriple(), 'release',
                     f'bindgen{EXE}'), os.path.join(install_dir, 'bin'))
    if sys.platform == 'win32':
        shutil.copy(os.path.join(llvm_dir, 'bin', f'libclang.dll'),
                    os.path.join(install_dir, 'bin'))
    elif sys.platform == 'darwin':
        shutil.copy(os.path.join(llvm_dir, 'lib', f'libclang.dylib'),
                    os.path.join(install_dir, 'lib'))
    else:
        # Can't replace symlinks so remove existing ones.
        for filename in os.listdir(os.path.join(install_dir, 'lib')):
            if filename.startswith('libclang.so'):
                os.remove(os.path.join(install_dir, 'lib', filename))
        for filename in os.listdir(os.path.join(llvm_dir, 'lib')):
            if filename.startswith('libclang.so'):
                shutil.copy(os.path.join(llvm_dir, 'lib', filename),
                            os.path.join(install_dir, 'lib'),
                            follow_symlinks=False)
    return 0


if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Builds the Crubit tool.

Builds the Crubit tools for generating Rust/C++ bindings.

This script must be run after //tools/rust/build_rust.py as it uses the outputs
of that script in the compilation of Crubit. It uses:
- The LLVM and Clang libraries and headers in `RUST_HOST_LLVM_INSTALL_DIR`.
- The rust toolchain binaries and libraries in `RUST_TOOLCHAIN_OUT_DIR`.

This script:
- Clones the Abseil repository, checks out a defined revision.
- Builds Abseil with Cmake.
- Clones the Crubit repository, checks out a defined revision.
- Builds Crubit's rs_bindings_from_cc with Cargo.
- Adds rs_bindings_from_cc and the Crubit support libraries into the
  toolchain package in `RUST_TOOLCHAIN_OUT_DIR`.

The cc_bindings_from_rs binary is not yet built, as there's no Cargo rules to build it yet.
'''

import argparse
import os
import platform
import shutil
import sys

from pathlib import Path

# Get variables and helpers from Clang update script
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'clang',
                 'scripts'))

from build import (AddCMakeToPath, AddZlibToPath, CheckoutGitRepo,
                   DownloadDebianSysroot, RunCommand, THIRD_PARTY_DIR)
from update import (RmTree)

from build_rust import (RUST_HOST_LLVM_INSTALL_DIR)
from update_rust import (CHROMIUM_DIR, ABSL_REVISION, CRUBIT_REVISION,
                         RUST_TOOLCHAIN_OUT_DIR)

ABSL_GIT = 'https://github.com/abseil/abseil-cpp'
CRUBIT_GIT = 'https://github.com/google/crubit'

ABSL_SRC_DIR = os.path.join(CHROMIUM_DIR, 'third_party',
                            'rust-toolchain-intermediate', 'absl')
ABSL_INSTALL_DIR = os.path.join(ABSL_SRC_DIR, 'install')
CRUBIT_SRC_DIR = os.path.join(CHROMIUM_DIR, 'third_party',
                              'rust-toolchain-intermediate', 'crubit')

EXE = '.exe' if sys.platform == 'win32' else ''


def BuildAbsl(env, debug):
    os.chdir(ABSL_SRC_DIR)

    configure_cmd = [
        'cmake',
        '-B',
        'out',
        '-GNinja',
        # Because Crubit is built with C++20.
        '-DCMAKE_CXX_STANDARD=20',
        f'-DCMAKE_INSTALL_PREFIX={ABSL_INSTALL_DIR}',
        '-DABSL_PROPAGATE_CXX_STD=ON',
        '-DABSL_BUILD_TESTING=OFF',
        '-DABSL_USE_GOOGLETEST_HEAD=OFF',
        # LLVM is built with static CRT. Make Abseil match it.
        '-DABSL_MSVC_STATIC_RUNTIME=ON',
    ]
    if not debug:
        configure_cmd.append('-DCMAKE_BUILD_TYPE=Release')

    RunCommand(configure_cmd, setenv=True, env=env)
    build_cmd = ['cmake', '--build', 'out', '--target', 'all']
    RunCommand(build_cmd, setenv=True, env=env)
    install_cmd = ['cmake', '--install', 'out']
    RunCommand(install_cmd, setenv=True, env=env)

    os.chdir(CHROMIUM_DIR)


def BuildCrubit(env, debug):
    os.chdir(CRUBIT_SRC_DIR)

    CRUBIT_BINS = ['rs_bindings_from_cc']

    build_cmd = ['cargo', 'build']
    for bin in CRUBIT_BINS:
        build_cmd += ['--bin', bin]
    if not debug:
        build_cmd.append('--release')
    RunCommand(build_cmd, setenv=True, env=env)

    print(f'Installing Crubit to {RUST_TOOLCHAIN_OUT_DIR} ...')
    target_dir = os.path.join(CRUBIT_SRC_DIR, 'target',
                              'debug' if debug else 'release')
    for bin in CRUBIT_BINS:
        bin = bin + EXE
        shutil.copy(os.path.join(target_dir, bin),
                    os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'bin', bin))

    support_build_dir = os.path.join(CRUBIT_SRC_DIR, 'support')
    support_out_dir = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'lib', 'crubit')
    if os.path.exists(support_out_dir):
        RmTree(support_out_dir)
    shutil.copytree(support_build_dir, support_out_dir)

    os.chdir(CHROMIUM_DIR)


def main():
    parser = argparse.ArgumentParser(
        description='Build and package Crubit tools')
    parser.add_argument(
        '--skip-checkout',
        action='store_true',
        help=('skip checking out source code. Useful for trying local'
              'changes'))
    parser.add_argument('--debug',
                        action='store_true',
                        help=('build Crubit in debug mode'))
    args, rest = parser.parse_known_args()
    assert (not rest)

    if not args.skip_checkout:
        CheckoutGitRepo("absl", ABSL_GIT, ABSL_REVISION, ABSL_SRC_DIR)
        CheckoutGitRepo("crubit", CRUBIT_GIT, CRUBIT_REVISION, CRUBIT_SRC_DIR)
    if sys.platform.startswith('linux'):
        arch = 'arm64' if platform.machine() == 'aarch64' else 'amd64'
        sysroot = DownloadDebianSysroot(arch, args.skip_checkout)

    llvm_bin_dir = os.path.join(RUST_HOST_LLVM_INSTALL_DIR, 'bin')
    rust_bin_dir = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'bin')

    AddCMakeToPath()

    env = os.environ

    path_trailing_sep = os.pathsep if env['PATH'] else ''
    env['PATH'] = (f'{llvm_bin_dir}{os.pathsep}'
                   f'{rust_bin_dir}{path_trailing_sep}'
                   f'{env["PATH"]}')

    if sys.platform == 'win32':
        # CMake on Windows doesn't like depot_tools's ninja.bat wrapper.
        ninja_dir = os.path.join(THIRD_PARTY_DIR, 'ninja')
        env['PATH'] = f'{ninja_dir}{os.pathsep}{env["PATH"]}'

    env['CXXFLAGS'] = ''
    env['RUSTFLAGS'] = ''

    if sys.platform == 'win32':
        env['CC'] = 'clang-cl'
        env['CXX'] = 'clang-cl'
    else:
        env['CC'] = 'clang'
        env['CXX'] = 'clang++'

    # We link with lld via clang, except on windows where we point to lld-link
    # directly.
    if sys.platform == 'win32':
        env['RUSTFLAGS'] += f' -Clinker=lld-link'
    else:
        env['RUSTFLAGS'] += f' -Clinker=clang'
        env['RUSTFLAGS'] += f' -Clink-arg=-fuse-ld=lld'

    if sys.platform == 'win32':
        # LLVM is built with static CRT. Make Rust match it.
        env['RUSTFLAGS'] += f' -Ctarget-feature=+crt-static'

    if sys.platform.startswith('linux'):
        sysroot_flag = (f'--sysroot={sysroot}' if sysroot else '')
        env['CXXFLAGS'] += f" {sysroot_flag}"
        env['RUSTFLAGS'] += f" -Clink-arg={sysroot_flag}"

    if sys.platform == 'darwin':
        import subprocess
        # The system/xcode compiler would find system SDK correctly, but
        # the Clang we've built does not. See
        # https://github.com/llvm/llvm-project/issues/45225
        sdk_path = subprocess.check_output(['xcrun', '--show-sdk-path'],
                                           text=True).rstrip()
        env['CXXFLAGS'] += f' -isysroot {sdk_path}'
        env['RUSTFLAGS'] += f' -Clink-arg=-isysroot -Clink-arg={sdk_path}'

    if sys.platform == 'win32':
        # LLVM depends on Zlib.
        zlib_dir = AddZlibToPath(dry_run=args.skip_checkout)
        env['CXXFLAGS'] += f' /I{zlib_dir}'
        env['RUSTFLAGS'] += f' -Clink-arg=/LIBPATH:{zlib_dir}'
        # Prevent deprecation warnings.
        env['CXXFLAGS'] += ' /D_CRT_SECURE_NO_DEPRECATE'

    BuildAbsl(env, args.debug)

    env['ABSL_INCLUDE_PATH'] = os.path.join(ABSL_INSTALL_DIR, 'include')
    env['ABSL_LIB_STATIC_PATH'] = os.path.join(ABSL_INSTALL_DIR, 'lib')
    env['CLANG_INCLUDE_PATH'] = os.path.join(RUST_HOST_LLVM_INSTALL_DIR,
                                             'include')
    env['CLANG_LIB_STATIC_PATH'] = os.path.join(RUST_HOST_LLVM_INSTALL_DIR,
                                                'lib')

    BuildCrubit(env, args.debug)

    return 0


if __name__ == '__main__':
    sys.exit(main())

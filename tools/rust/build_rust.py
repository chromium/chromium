#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Assembles a Rust toolchain in-tree linked against in-tree LLVM.

Builds a Rust toolchain bootstrapped from an untrusted rustc build.

Rust has an official boostrapping build. At a high level:
1. A "stage 0" Rust is downloaded from Rust's official server. This
   is one major release before the version being built. E.g. if building trunk
   the latest beta is downloaded. If building stable 1.57.0, stage0 is stable
   1.56.1.
2. Stage 0 libstd is built. This is different than the libstd downloaded above.
3. Stage 1 rustc is built with rustc from (1) and libstd from (2)
2. Stage 1 libstd is built with stage 1 rustc. Later artifacts built with
   stage 1 rustc are built with stage 1 libstd.

Further stages are possible and continue as expected. Additionally, the build
can be extensively customized. See for details:
https://rustc-dev-guide.rust-lang.org/building/bootstrapping.html

This script clones the Rust repository, checks it out to a defined revision,
then builds stage 1 rustc and libstd using the LLVM build from
//tools/clang/scripts/build.py or clang-libs fetched from
//tools/clang/scripts/update.py.

Ideally our build would begin with our own trusted stage0 rustc. As it is
simpler, for now we use an official build.

TODO(https://crbug.com/1245714): Do a proper 3-stage build

'''

import argparse
import collections
import hashlib
import platform
import os
import pipes
import shutil
import string
import subprocess
import sys

from pathlib import Path

# Get variables and helpers from Clang update script.
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'clang',
                 'scripts'))

from build import (AddCMakeToPath, AddOpenSSLToEnv, AddZlibToPath,
                   GetLibXml2Dirs, RunCommand)
from update import (CLANG_REVISION, CLANG_SUB_REVISION, LLVM_BUILD_DIR,
                    GetDefaultHostOs, RmTree, UpdatePackage)
import build

from update_rust import (CHROMIUM_DIR, RUST_REVISION, RUST_SUB_REVISION,
                         RUST_TOOLCHAIN_OUT_DIR, STAGE0_JSON_SHA256,
                         THIRD_PARTY_DIR, VERSION_STAMP_PATH,
                         GetPackageVersionForBuild)

RUST_GIT_URL = 'https://github.com/rust-lang/rust/'

RUST_SRC_DIR = os.path.join(THIRD_PARTY_DIR, 'rust_src', 'src')
STAGE0_JSON_PATH = os.path.join(RUST_SRC_DIR, 'src', 'stage0.json')
# Download crates.io dependencies to rust-src subdir (rather than $HOME/.cargo)
CARGO_HOME_DIR = os.path.join(RUST_SRC_DIR, 'cargo-home')
RUST_SRC_VERSION_FILE_PATH = os.path.join(RUST_SRC_DIR, 'version')
RUST_SRC_GIT_COMMIT_INFO_FILE_PATH = os.path.join(RUST_SRC_DIR,
                                                  'git-commit-info')
RUST_TOOLCHAIN_LIB_DIR = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'lib')
RUST_TOOLCHAIN_SRC_DIST_DIR = os.path.join(RUST_TOOLCHAIN_LIB_DIR, 'rustlib',
                                           'src', 'rust')
RUST_TOOLCHAIN_SRC_DIST_VENDOR_DIR = os.path.join(RUST_TOOLCHAIN_SRC_DIST_DIR,
                                                  'vendor')
RUST_CONFIG_TEMPLATE_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), 'config.toml.template')
RUST_SRC_VENDOR_DIR = os.path.join(RUST_SRC_DIR, 'vendor')

if sys.platform == 'win32':
    LD_PATH_FLAG = '/LIBPATH:'
else:
    LD_PATH_FLAG = '-L'

# Desired tools and libraries in our Rust toolchain.
DISTRIBUTION_ARTIFACTS = [
    'cargo', 'clippy', 'compiler/rustc', 'library/std', 'rust-analyzer',
    'rustfmt', 'src'
]

# Which test suites to run. Any failure will fail the build.
TEST_SUITES = [
    'library/std',
    'src/test/codegen',
    'src/test/ui',
]

EXCLUDED_TESTS = [
    # Temporarily disabled due to https://github.com/rust-lang/rust/issues/94322
    'src/test/ui/numeric/numeric-cast.rs',
    # Temporarily disabled due to https://github.com/rust-lang/rust/issues/96497
    'src/test/codegen/issue-96497-slice-size-nowrap.rs',
    # TODO(crbug.com/1347563): Re-enable when fixed.
    'src/test/codegen/sanitizer-cfi-emit-type-checks.rs',
    'src/test/codegen/sanitizer-cfi-emit-type-metadata-itanium-cxx-abi.rs',
]


def VerifyStage0JsonHash():
    hasher = hashlib.sha256()
    with open(STAGE0_JSON_PATH, 'rb') as input:
        hasher.update(input.read())
    actual_hash = hasher.hexdigest()

    if actual_hash == STAGE0_JSON_SHA256:
        return

    print('src/stage0.json hash is different than expected!')
    print('Expected hash: ' + STAGE0_JSON_SHA256)
    print('Actual hash:   ' + actual_hash)
    sys.exit(1)


def Configure(llvm_bins_path, llvm_libs_root):
    # Read the config.toml template file...
    with open(RUST_CONFIG_TEMPLATE_PATH, 'r') as input:
        template = string.Template(input.read())

    def quote_string(s: str):
        return s.replace('\\', '\\\\').replace('"', '\\"')

    subs = {}
    subs['INSTALL_DIR'] = quote_string(str(RUST_TOOLCHAIN_OUT_DIR))
    subs['LLVM_LIB_ROOT'] = quote_string(str(llvm_libs_root))
    subs['LLVM_BIN'] = quote_string(str(llvm_bins_path))
    subs['PACKAGE_VERSION'] = GetPackageVersionForBuild()

    # ...and apply substitutions, writing to config.toml in Rust tree.
    with open(os.path.join(RUST_SRC_DIR, 'config.toml'), 'w') as output:
        output.write(template.substitute(subs))


def RunXPy(sub, args, llvm_bins_path, zlib_path, libxml2_dirs, build_mac_arm,
           gcc_toolchain_path, verbose):
    ''' Run x.py, Rust's build script'''
    # We append to these flags, make sure they exist.
    ENV_FLAGS = [
        'CFLAGS',
        'CXXFLAGS',
        'LDFLAGS',
        'RUSTFLAGS_BOOTSTRAP',
        'RUSTFLAGS_NOT_BOOTSTRAP',
    ]

    RUSTENV = collections.defaultdict(str, os.environ)
    for f in ENV_FLAGS:
        RUSTENV.setdefault(f, '')

    # The AR, CC, CXX flags control the C/C++ compiler used through the `cc`
    # crate. There are also C/C++ targets that are part of the Rust toolchain
    # build for which the tool is controlled from `config.toml`, so these must
    # be duplicated there.

    if sys.platform == 'win32':
        RUSTENV['AR'] = os.path.join(llvm_bins_path, 'llvm-lib.exe')
        RUSTENV['CC'] = os.path.join(llvm_bins_path, 'clang-cl.exe')
        RUSTENV['CXX'] = os.path.join(llvm_bins_path, 'clang-cl.exe')
    else:
        RUSTENV['AR'] = os.path.join(llvm_bins_path, 'llvm-ar')
        RUSTENV['CC'] = os.path.join(llvm_bins_path, 'clang')
        RUSTENV['CXX'] = os.path.join(llvm_bins_path, 'clang++')

    if sys.platform == 'darwin':
        # The system/xcode compiler would find system SDK correctly, but
        # the Clang we've built does not. See
        # https://github.com/llvm/llvm-project/issues/45225
        sdk_path = subprocess.check_output(['xcrun', '--show-sdk-path'],
                                           text=True).rstrip()
        RUSTENV['CFLAGS'] += f' -isysroot {sdk_path}'
        RUSTENV['CXXFLAGS'] += f' -isysroot {sdk_path}'
        RUSTENV['LDFLAGS'] += f' -isysroot {sdk_path}'
        RUSTENV['RUSTFLAGS_BOOTSTRAP'] += (f' -Clink-arg=-isysroot {sdk_path}')
        RUSTENV['RUSTFLAGS_NOT_BOOTSTRAP'] += (
            f' -Clink-arg=-isysroot {sdk_path}')

    if zlib_path:
        RUSTENV['CFLAGS'] += f' -I{zlib_path}'
        RUSTENV['CXXFLAGS'] += f' -I{zlib_path}'
        RUSTENV['LDFLAGS'] += f' {LD_PATH_FLAG}{zlib_path}'
        RUSTENV['RUSTFLAGS_BOOTSTRAP'] += (f' -Clink-arg='
                                           f'{LD_PATH_FLAG}{zlib_path}')
        RUSTENV['RUSTFLAGS_NOT_BOOTSTRAP'] += (f' -Clink-arg='
                                               f'{LD_PATH_FLAG}{zlib_path}')

    if libxml2_dirs:
        RUSTENV['CFLAGS'] += f' -I{libxml2_dirs.include_dir}'
        RUSTENV['CXXFLAGS'] += f' -I{libxml2_dirs.include_dir}'
        RUSTENV['LDFLAGS'] += f' {LD_PATH_FLAG}{libxml2_dirs.lib_dir}'
        RUSTENV['RUSTFLAGS_BOOTSTRAP'] += (
            f' -Clink-arg='
            f'{LD_PATH_FLAG}{libxml2_dirs.lib_dir}')
        RUSTENV['RUSTFLAGS_NOT_BOOTSTRAP'] += (
            f' -Clink-arg='
            f'{LD_PATH_FLAG}{libxml2_dirs.lib_dir}')

    if gcc_toolchain_path:
        # We use these flags to avoid linking with the system libstdc++.
        gcc_toolchain_flag = (f'--gcc-toolchain={gcc_toolchain_path}')
        RUSTENV['CFLAGS'] += f' {gcc_toolchain_flag}'
        RUSTENV['CXXFLAGS'] += f' {gcc_toolchain_flag}'
        RUSTENV['LDFLAGS'] += f' {gcc_toolchain_flag}'
        # A `-Clink-arg=<foo>` arg passes `foo`` to the linker invovation.
        RUSTENV['RUSTFLAGS_BOOTSTRAP'] += f' -Clink-arg={gcc_toolchain_flag}'
        RUSTENV[
            'RUSTFLAGS_NOT_BOOTSTRAP'] += f' -Clink-arg={gcc_toolchain_flag}'
        RUSTENV['RUSTFLAGS_BOOTSTRAP'] += (
            f' -L native={gcc_toolchain_path}/lib64')
        RUSTENV['RUSTFLAGS_NOT_BOOTSTRAP'] += (
            f' -L native={gcc_toolchain_path}/lib64')

    # Cargo normally stores files in $HOME. Override this.
    RUSTENV['CARGO_HOME'] = CARGO_HOME_DIR

    # This enables the compiler to produce Mac ARM binaries.
    if build_mac_arm:
        args.extend(['--target', 'aarch64-apple-darwin'])

    os.chdir(RUST_SRC_DIR)
    cmd = [sys.executable, 'x.py', sub]
    if verbose and verbose > 0:
        cmd.append('-' + verbose * 'v')
    RunCommand(cmd + args, msvc_arch='x64', env=RUSTENV)


# Get arguments to run desired test suites, minus disabled tests.
def GetTestArgs():
    args = TEST_SUITES
    for excluded in EXCLUDED_TESTS:
        args.append('--skip')
        args.append(excluded)
    return args


def GetVersionStamp():
    # We must generate a version stamp that contains the expected `rustc
    # --version` output. This contains the Rust release version, git commit data
    # that the nightly tarball was generated from, and chromium-specific package
    # information.
    with open(RUST_SRC_VERSION_FILE_PATH) as version_file:
        rust_version = version_file.readline().rstrip()

    return ('rustc %s (%s chromium)\n' %
            (rust_version, GetPackageVersionForBuild()))


def main():
    parser = argparse.ArgumentParser(
        description='Build and package Rust toolchain')
    parser.add_argument('-v',
                        '--verbose',
                        action='count',
                        help='run subcommands with verbosity')
    parser.add_argument('--build-mac-arm',
                        action='store_true',
                        help='Build arm binaries. Only valid on macOS.')
    parser.add_argument(
        '--verify-stage0-hash',
        action='store_true',
        help=
        'checkout Rust, verify the stage0 hash, then quit without building. '
        'Will print the actual hash if different than expected.')
    parser.add_argument('--skip-clean',
                        action='store_true',
                        help='skip x.py clean step')
    parser.add_argument('--skip-test',
                        action='store_true',
                        help='skip running rustc and libstd tests')
    parser.add_argument('--skip-install',
                        action='store_true',
                        help='do not install to RUST_TOOLCHAIN_OUT_DIR')
    parser.add_argument(
        '--fetch-llvm-libs',
        action='store_true',
        help='fetch Clang/LLVM libs and extract into LLVM_BUILD_DIR. Useless '
        'without --use-final-llvm-build-dir.')
    parser.add_argument(
        '--use-final-llvm-build-dir',
        action='store_true',
        help='use libs in LLVM_BUILD_DIR instead of LLVM_BOOTSTRAP_DIR. Useful '
        'with --fetch-llvm-libs for local builds.')
    parser.add_argument(
        '--run-xpy',
        action='store_true',
        help='run x.py command in configured Rust checkout. Quits after '
        'running specified command, skipping all normal build steps. For '
        'debugging. Running x.py directly will not set the appropriate env '
        'variables nor update config.toml')
    args, rest = parser.parse_known_args()

    if args.build_mac_arm and sys.platform != 'darwin':
        print('--build-mac-arm only valid on macOS')
        return 1
    if args.build_mac_arm and platform.machine() == 'arm64':
        print('--build-mac-arm only valid on intel to cross-build arm')
        return 1

    # Get the LLVM root for libs. We use LLVM_BUILD_DIR tools either way.
    #
    # TODO(https://crbug.com/1245714): use LTO libs from LLVM_BUILD_DIR for
    # stage 2+.
    if args.use_final_llvm_build_dir:
        llvm_libs_root = LLVM_BUILD_DIR
    else:
        llvm_libs_root = build.LLVM_BOOTSTRAP_DIR

    # If we're building for Mac ARM on an x86_64 Mac, we can't use the final
    # clang binaries as they don't have x86_64 support. Building them with that
    # support would blow up their size a lot. So, we use the bootstrap binaries
    # until such time as the Mac ARM builder becomes an actual Mac ARM machine.
    if not args.build_mac_arm:
        llvm_bins_path = os.path.join(LLVM_BUILD_DIR, 'bin')
    else:
        llvm_bins_path = os.path.join(build.LLVM_BOOTSTRAP_DIR, 'bin')

    VerifyStage0JsonHash()
    if args.verify_stage0_hash:
        # The above function exits and prints the actual hash if verification
        # failed so we just quit here; if we reach this point, the hash is
        # valid.
        return 0

    if args.fetch_llvm_libs:
        UpdatePackage('clang-libs', GetDefaultHostOs())

    args.gcc_toolchain = None
    if sys.platform.startswith('linux'):
        # Fetch GCC package to build against same libstdc++ as Clang. This
        # function will only download it if necessary, and it will set the
        # `args.gcc_toolchain` if so.
        build.MaybeDownloadHostGcc(args)

    # Set up config.toml in Rust source tree to configure build.
    Configure(llvm_bins_path, llvm_libs_root)

    AddCMakeToPath()

    # Require zlib compression.
    if sys.platform == 'win32':
        zlib_path = AddZlibToPath()
    else:
        zlib_path = None

    # libxml2 is built when building LLVM, so we use that.
    if sys.platform == 'win32':
        libxml2_dirs = GetLibXml2Dirs()
    else:
        libxml2_dirs = None

    # Cargo requires OpenSSL to build, and it's not already present on Mac
    # builders.
    if sys.platform == 'darwin':
        AddOpenSSLToEnv(args.build_mac_arm)

    if args.run_xpy:
        if rest[0] == '--':
            rest = rest[1:]
        RunXPy(rest[0], rest[1:], llvm_bins_path, zlib_path, libxml2_dirs,
               args.build_mac_arm, args.gcc_toolchain, args.verbose)
        return 0
    else:
        assert not rest

    if not args.skip_clean:
        print('Cleaning build artifacts...')
        RunXPy('clean', [], llvm_bins_path, zlib_path, libxml2_dirs,
               args.build_mac_arm, args.gcc_toolchain, args.verbose)

    if not args.skip_test:
        print('Running stage 2 tests...')
        # Run a subset of tests. Tell x.py to keep the rustc we already built.
        RunXPy('test', GetTestArgs(), llvm_bins_path, zlib_path, libxml2_dirs,
               args.build_mac_arm, args.gcc_toolchain, args.verbose)

    targets = [
        'library/proc_macro', 'library/std', 'src/tools/cargo',
        'src/tools/clippy', 'src/tools/rustfmt'
    ]

    # Build stage 2 compiler, tools, and libraries. This should reuse earlier
    # stages from the test command (if run).
    print('Building stage 2 artifacts...')
    RunXPy('build', ['--stage', '2'] + targets, llvm_bins_path, zlib_path,
           libxml2_dirs, args.build_mac_arm, args.gcc_toolchain, args.verbose)

    if args.skip_install:
        # Rust is fully built. We can quit.
        return 0

    print(f'Installing to {RUST_TOOLCHAIN_OUT_DIR} ...')
    # Clean output directory.
    if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
        shutil.rmtree(RUST_TOOLCHAIN_OUT_DIR)

    RunXPy('install', DISTRIBUTION_ARTIFACTS, llvm_bins_path, zlib_path,
           libxml2_dirs, args.gcc_toolchain, args.verbose)

    with open(VERSION_STAMP_PATH, 'w') as stamp:
        stamp.write(GetVersionStamp())

    return 0

    # TODO(crbug.com/1342708): fix vendoring and re-enable.
    # x.py installed library sources to our toolchain directory. We also need to
    # copy the vendor directory so Chromium checkouts have all the deps needed
    # to build std.
    # shutil.copytree(RUST_SRC_VENDOR_DIR, RUST_TOOLCHAIN_SRC_DIST_VENDOR_DIR)


if __name__ == '__main__':
    sys.exit(main())

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

from build import (AddCMakeToPath, RunCommand)
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


def Configure(llvm_libs_root):
    # Read the config.toml template file...
    with open(RUST_CONFIG_TEMPLATE_PATH, 'r') as input:
        template = string.Template(input.read())

    def quote_string(s: str):
        return s.replace('\\', '\\\\').replace('"', '\\"')

    subs = {}
    subs['INSTALL_DIR'] = quote_string(str(RUST_TOOLCHAIN_OUT_DIR))
    subs['LLVM_ROOT'] = quote_string(str(llvm_libs_root))
    subs['PACKAGE_VERSION'] = GetPackageVersionForBuild()

    # ...and apply substitutions, writing to config.toml in Rust tree.
    with open(os.path.join(RUST_SRC_DIR, 'config.toml'), 'w') as output:
        output.write(template.substitute(subs))


def RunXPy(sub, args, gcc_toolchain_path, verbose):
    ''' Run x.py, Rust's build script'''
    RUSTENV = collections.defaultdict(str, os.environ)

    ##### For C/C++ compilation steps #####
    # The Rust toolchain does include some C/C++ code, and these env vars
    # control the compilation and library making for that code.

    clang_path = os.path.join(LLVM_BUILD_DIR, 'bin')
    if sys.platform == 'win32':
        # The Rust toolchain build requires the use of link.exe already (it is
        # the well-lit path, and we don't override the Rust linker when building
        # the toolchain here), so we could use it for linking the small bits of
        # C/C++ inside the Rust build too.
        #
        # That said, the `config.toml.template` file can override the linker for
        # the Rust toolchain build.
        #
        # The `cc` crate wants to use link.exe by default for making static
        # libs (the `AR` env var) but if clang-cl is being used to compile,
        # then it wants to use llvm-lib.
        #
        # It's not totally clear if we ship llvm-lib, but it seems in practice
        # that we have it available on the LLVM toolchain builders. Otherwise,
        # we would need to use `lld-link /lib` and that doesn't work
        # at the moment as the `cc` crate doesn't consume `ARFLAGS`:
        # https://github.com/rust-lang/cc-rs/issues/762
        #
        # So, since we're using clang-cl, we point to `llvm-lib`, though we
        # could equally let the `cc` crate find it (it looks in the same path
        # as the compiler on Windows as of the time of this writing).
        #
        # We don't set a final linker with `LD`, as either the default works,
        # or there are C executables or shared libs compiled during the Rust
        # toolchain build.
        RUSTENV['AR'] = os.path.join(clang_path, 'llvm-lib')
        RUSTENV['CC'] = os.path.join(clang_path, 'clang-cl')
        RUSTENV['CXX'] = os.path.join(clang_path, 'clang-cl')
    else:
        RUSTENV['AR'] = os.path.join(clang_path, 'llvm-ar')
        RUSTENV['CC'] = os.path.join(clang_path, 'clang')
        RUSTENV['CXX'] = os.path.join(clang_path, 'clang++')

    if gcc_toolchain_path:
        # We use these flags to avoid linking with the system libstdc++.
        gcc_toolchain_flag = (f'--gcc-toolchain={gcc_toolchain_path}')
        RUSTENV['CFLAGS'] += f' {gcc_toolchain_flag}'
        RUSTENV['CXXFLAGS'] += f' {gcc_toolchain_flag}'
        RUSTENV['LDFLAGS'] += f' {gcc_toolchain_flag}'

    ##### For Rust compilation steps #####
    # These env vars set arguments passed to the rust compiler when building
    # Rust target.
    #
    # A `-Clink-arg=<foo>` arg passes `foo`` to the linker invovation.

    RUSTENV['RUSTFLAGS_BOOTSTRAP'] = ''
    if gcc_toolchain_path:
        RUSTENV['RUSTFLAGS_BOOTSTRAP'] += f' -Clink-arg={gcc_toolchain_flag} '
        RUSTENV['RUSTFLAGS_BOOTSTRAP'] += (
            f' -L native={gcc_toolchain_path}/lib64')
    RUSTENV['RUSTFLAGS_NOT_BOOTSTRAP'] = RUSTENV['RUSTFLAGS_BOOTSTRAP']

    ##### Do the build now #####

    # Cargo normally stores files in $HOME. Override this.
    RUSTENV['CARGO_HOME'] = CARGO_HOME_DIR

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

    # Get the LLVM root for libs. We use LLVM_BUILD_DIR tools either way.
    #
    # TODO(https://crbug.com/1245714): use LTO libs from LLVM_BUILD_DIR for
    # stage 2+.
    if args.use_final_llvm_build_dir:
        llvm_libs_root = LLVM_BUILD_DIR
    else:
        llvm_libs_root = build.LLVM_BOOTSTRAP_DIR

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
    Configure(llvm_libs_root)

    AddCMakeToPath()

    if args.run_xpy:
        if rest[0] == '--':
            rest = rest[1:]
        RunXPy(rest[0], rest[1:], args.gcc_toolchain, args.verbose)
        return 0
    else:
        assert not rest

    if not args.skip_clean:
        print('Cleaning build artifacts...')
        RunXPy('clean', [], args.gcc_toolchain, args.verbose)

    if not args.skip_test:
        print('Running stage 2 tests...')
        # Run a subset of tests. Tell x.py to keep the rustc we already built.
        RunXPy('test', GetTestArgs(), args.gcc_toolchain, args.verbose)

    targets = [
        'library/proc_macro', 'library/std', 'src/tools/cargo',
        'src/tools/clippy', 'src/tools/rustfmt'
    ]

    # Build stage 2 compiler, tools, and libraries. This should reuse earlier
    # stages from the test command (if run).
    print('Building stage 2 artifacts...')
    RunXPy('build', ['--stage', '2'] + targets, args.gcc_toolchain,
           args.verbose)

    if args.skip_install:
        # Rust is fully built. We can quit.
        return 0

    print(f'Installing to {RUST_TOOLCHAIN_OUT_DIR} ...')
    # Clean output directory.
    if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
        shutil.rmtree(RUST_TOOLCHAIN_OUT_DIR)

    RunXPy('install', DISTRIBUTION_ARTIFACTS, args.gcc_toolchain, args.verbose)

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

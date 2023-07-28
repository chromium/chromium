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
import base64
import collections
import hashlib
import json
import platform
import os
import re
import shutil
import string
import subprocess
import sys
import urllib

from pathlib import Path

# Get variables and helpers from Clang update script.
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'clang',
                 'scripts'))

from build import (AddCMakeToPath, AddZlibToPath, CheckoutGitRepo,
                   DownloadDebianSysroot, GetLibXml2Dirs, LLVM_BUILD_TOOLS_DIR,
                   RunCommand)
from update import (CHROMIUM_DIR, DownloadAndUnpack, EnsureDirExists,
                    GetDefaultHostOs, RmTree, UpdatePackage)

from update_rust import (RUST_REVISION, RUST_TOOLCHAIN_OUT_DIR,
                         STAGE0_JSON_SHA256, THIRD_PARTY_DIR, VERSION_SRC_PATH,
                         GetRustClangRevision)

EXCLUDED_TESTS = [
    # https://github.com/rust-lang/rust/issues/45222 which appears to have
    # regressed as of a recent LLVM update. This test is purely performance
    # related, not correctness.
    os.path.join('tests', 'codegen', 'issue-45222.rs'),
    # https://github.com/rust-lang/rust/issues/96497
    os.path.join('tests', 'codegen', 'issue-96497-slice-size-nowrap.rs'),
    # https://github.com/rust-lang/rust/issues/109671 the test is being
    # optimized in newer LLVM which breaks its expectations.
    os.path.join('tests', 'ui', 'abi', 'stack-protector.rs'),
    # https://github.com/rust-lang/rust/issues/109672 the second panic in a
    # double-panic is being optimized out (reasonably correctly) by newer LLVM.
    os.path.join('tests', 'ui', 'backtrace.rs'),
    # https://github.com/rust-lang/rust/issues/94322 large output from
    # compiletests is breaking json parsing of the results.
    os.path.join('tests', 'ui', 'numeric', 'numeric-cast.rs'),
]
EXCLUDED_TESTS_WINDOWS = [
    # https://github.com/rust-lang/rust/issues/96464
    os.path.join('tests', 'codegen', 'vec-shrink-panik.rs'),
]

CLANG_SCRIPTS_DIR = os.path.join(CHROMIUM_DIR, 'tools', 'clang', 'scripts')

RUST_GIT_URL = ('https://chromium.googlesource.com/external/' +
                'github.com/rust-lang/rust')

RUST_SRC_DIR = os.path.join(THIRD_PARTY_DIR, 'rust-src')
RUST_BOOTSTRAP_DIST_RS = os.path.join(RUST_SRC_DIR, 'src', 'bootstrap',
                                      'dist.rs')
STAGE0_JSON_PATH = os.path.join(RUST_SRC_DIR, 'src', 'stage0.json')
# Download crates.io dependencies to rust-src subdir (rather than $HOME/.cargo)
CARGO_HOME_DIR = os.path.join(RUST_SRC_DIR, 'cargo-home')
RUST_SRC_VERSION_FILE_PATH = os.path.join(RUST_SRC_DIR, 'src', 'version')
RUST_SRC_GIT_COMMIT_INFO_FILE_PATH = os.path.join(RUST_SRC_DIR,
                                                  'git-commit-info')
RUST_TOOLCHAIN_LIB_DIR = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'lib')
RUST_TOOLCHAIN_SRC_DIST_DIR = os.path.join(RUST_TOOLCHAIN_LIB_DIR, 'rustlib',
                                           'src', 'rust')
RUST_TOOLCHAIN_SRC_DIST_VENDOR_DIR = os.path.join(RUST_TOOLCHAIN_SRC_DIST_DIR,
                                                  'vendor')
RUST_CONFIG_TEMPLATE_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), 'config.toml.template')
RUST_CARGO_CONFIG_TEMPLATE_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), 'cargo-config.toml.template')
RUST_SRC_VENDOR_DIR = os.path.join(RUST_SRC_DIR, 'vendor')

RUST_HOST_LLVM_BUILD_DIR = os.path.join(CHROMIUM_DIR, 'third_party',
                                        'rust-toolchain-intermediate',
                                        'llvm-host-build')
RUST_HOST_LLVM_INSTALL_DIR = os.path.join(CHROMIUM_DIR, 'third_party',
                                          'rust-toolchain-intermediate',
                                          'llvm-host-install')
RUST_CROSS_TARGET_LLVM_BUILD_DIR = os.path.join(CHROMIUM_DIR, 'third_party',
                                                'rust-toolchain-intermediate',
                                                'llvm-target-build')
RUST_CROSS_TARGET_LLVM_INSTALL_DIR = os.path.join(
    CHROMIUM_DIR, 'third_party', 'rust-toolchain-intermediate',
    'llvm-target-install')

# CIPD Versions from:
# - List all platforms
# cipd ls infra/3pp/static_libs/openssl/
# - Find all versions for a platform
# cipd instances infra/3pp/static_libs/openssl/linux-amd64
# - Find the version tag for a version
# cipd desc infra/3pp/static_libs/openssl/linux-amd64 --version <instance id>
# - A version tag looks like: `version:2@1.1.1j.chromium.2` and we
#   store the part after the `2@` here.
CIPD_DOWNLOAD_URL = f'https://chrome-infra-packages.appspot.com/dl'
OPENSSL_CIPD_LINUX_AMD_PATH = 'infra/3pp/static_libs/openssl/linux-amd64'
OPENSSL_CIPD_LINUX_AMD_VERSION = '1.1.1j.chromium.2'
OPENSSL_CIPD_MAC_AMD_PATH = 'infra/3pp/static_libs/openssl/mac-amd64'
OPENSSL_CIPD_MAC_AMD_VERSION = '1.1.1j.chromium.2'
OPENSSL_CIPD_MAC_ARM_PATH = 'infra/3pp/static_libs/openssl/mac-amd64'
OPENSSL_CIPD_MAC_ARM_VERSION = '1.1.1j.chromium.2'
# TODO(crbug.com/1271215): Pull Windows OpenSSL from 3pp when it exists.

if sys.platform == 'win32':
    LD_PATH_FLAG = '/LIBPATH:'
else:
    LD_PATH_FLAG = '-L'

BUILD_TARGETS = [
    'library/proc_macro', 'library/std', 'src/tools/cargo', 'src/tools/clippy',
    'src/tools/rustfmt'
]

# Desired tools and libraries in our Rust toolchain.
DISTRIBUTION_ARTIFACTS = [
    'cargo', 'clippy', 'compiler/rustc', 'library/std', 'rust-analyzer',
    'rustfmt', 'src'
]
DISTRIBUTION_ARTIFACTS_SKIPPED_CROSS_COMPILE = [
    # Cargo depends on OpenSSL and we don't have a cross-compile of the
    # library available. This means gnrt will not function on M1 macs until
    # we fix this, or stop depending on cross-compile.
    'cargo',
    # When cross-compiling this fails to build terribly, as it can't even
    # find the Rust stdlib.
    'rust-analyzer',
]

# Which test suites to run. Any failure will fail the build.
TEST_SUITES = [
    'library/std',
    'tests/codegen',
    'tests/ui',
]

def AddOpenSSLToEnv(build_mac_arm):
    """Download OpenSSL, and add to OPENSSL_DIR."""
    ssl_dir = os.path.join(LLVM_BUILD_TOOLS_DIR, 'openssl')

    if sys.platform == 'darwin':
        if platform.machine() == 'arm64' or build_mac_arm:
            ssl_url = (f'{CIPD_DOWNLOAD_URL}/{OPENSSL_CIPD_MAC_ARM_PATH}'
                       f'/+/version:2@{OPENSSL_CIPD_MAC_ARM_VERSION}')
        else:
            ssl_url = (f'{CIPD_DOWNLOAD_URL}/{OPENSSL_CIPD_MAC_AMD_PATH}'
                       f'/+/version:2@{OPENSSL_CIPD_MAC_AMD_VERSION}')
    elif sys.platform == 'win32':
        ssl_url = (f'{CIPD_DOWNLOAD_URL}/{OPENSSL_CIPD_WIN_AMD_PATH}'
                   f'/+/version:2@{OPENSSL_CIPD_WIN_AMD_VERSION}')
    else:
        ssl_url = (f'{CIPD_DOWNLOAD_URL}/{OPENSSL_CIPD_LINUX_AMD_PATH}'
                   f'/+/version:2@{OPENSSL_CIPD_LINUX_AMD_VERSION}')

    if os.path.exists(ssl_dir):
        RmTree(ssl_dir)
    DownloadAndUnpack(ssl_url, ssl_dir, is_known_zip=True)
    os.environ['OPENSSL_DIR'] = ssl_dir
    return ssl_dir


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


def FetchBetaPackage(name, rust_git_hash, triple=None):
    '''Downloads the beta package specified for the compiler build

    If `triple` is not specified, it downloads a package for the current
    machine's architecture.

    Unpacks the package and returns the path to root of the package.
    '''
    triple = triple if triple else RustTargetTriple()
    filename = f'{name}-beta-{triple}'

    # Pull the stage0 JSON to find the package intended to be used to
    # build this version of the Rust compiler.
    STAGE0_JSON_URL = (
        'https://chromium.googlesource.com/external/github.com/'
        'rust-lang/rust/+/{GIT_HASH}/src/stage0.json?format=TEXT')
    base64_text = urllib.request.urlopen(
        STAGE0_JSON_URL.format(GIT_HASH=rust_git_hash)).read().decode("utf-8")
    stage0 = json.loads(base64.b64decode(base64_text))

    # The stage0 JSON contains the path to all tarballs it uses binaries from.
    for k in stage0['checksums_sha256'].keys():
        if k.endswith(filename + '.tar.gz'):
            package_tgz = k

    server = stage0['config']['dist_server']
    DownloadAndUnpack(f'{server}/{package_tgz}', LLVM_BUILD_TOOLS_DIR)
    return os.path.join(LLVM_BUILD_TOOLS_DIR, filename)


def InstallBetaPackage(package_dir, install_dir):
    args = [
        f'--destdir={install_dir}',
        f'--prefix=',
    ]
    if sys.platform.startswith('linux'):
        # Avoid warnings due to not running as root.
        args += ['--disable-ldconfig']
    RunCommand([os.path.join(package_dir, 'install.sh')] + args)


def CargoVendor(cargo_bin):
    '''Runs `cargo vendor` to pull down dependencies.'''
    os.chdir(RUST_SRC_DIR)

    # Some Submodules are part of the workspace and need to exist (so we can
    # read their Cargo.toml files) before we can vendor their deps.
    submod_cmd = [
        'git', 'submodule', 'update', '--init', '--recursive', '--depth', '1'
    ]
    RunWithRetry(submod_cmd, 'git submodule')

    vendor_env = os.environ
    # The Cargo.toml files in the Rust toolchain may use nightly Cargo
    # features, but the cargo binary is beta. This env var enables the
    # beta cargo binary to allow nightly features anyway.
    # https://github.com/rust-lang/rust/commit/2e52f4deb0544480b6aefe2c0cc1e6f3c893b081
    vendor_env['RUSTC_BOOTSTRAP'] = '1'

    vendor_cmd = [
        cargo_bin,
        'vendor',
        '--locked',
        '--versioned-dirs',
    ]
    RunWithRetry(vendor_cmd, 'cargo vendor')

    os.chdir(CHROMIUM_DIR)


def RunWithRetry(command, name):
    '''Run a command, retrying a few times then aborting if it fails.'''
    for i in range(0, 3):
        if RunCommand(command, fail_hard=False):
            return
        elif i < 2:
            print(f'failed {name}, retrying...')
        else:
            sys.exit(1)

class XPy:
    ''' Runner for x.py, Rust's build script. Holds shared state between x.py
    runs. '''

    def __init__(self, zlib_path, libxml2_dirs, build_mac_arm, debian_sysroot,
                 verbose):
        self._env = collections.defaultdict(str, os.environ)
        self._build_mac_arm = build_mac_arm
        self._verbose = verbose
        self._llvm_bins_path = os.path.join(RUST_HOST_LLVM_INSTALL_DIR, 'bin')

        # We append to these flags, make sure they exist.
        ENV_FLAGS = [
            'CFLAGS',
            'CXXFLAGS',
            'LDFLAGS',
            'RUSTFLAGS_BOOTSTRAP',
            'RUSTFLAGS_NOT_BOOTSTRAP',
            'RUSTDOCFLAGS',
        ]

        self._env = collections.defaultdict(str, os.environ)
        for f in ENV_FLAGS:
            self._env.setdefault(f, '')

        # The AR, CC, CXX flags control the C/C++ compiler used through the `cc`
        # crate. There are also C/C++ targets that are part of the Rust
        # toolchain build for which the tool is controlled from `config.toml`,
        # so these must be duplicated there.

        if sys.platform == 'win32':
            self._env['AR'] = os.path.join(self._llvm_bins_path,
                                           'llvm-lib.exe')
            self._env['CC'] = os.path.join(self._llvm_bins_path,
                                           'clang-cl.exe')
            self._env['CXX'] = os.path.join(self._llvm_bins_path,
                                            'clang-cl.exe')
            self._env['LD'] = os.path.join(self._llvm_bins_path,
                                           'lld-link.exe')
        else:
            self._env['AR'] = os.path.join(self._llvm_bins_path, 'llvm-ar')
            self._env['CC'] = os.path.join(self._llvm_bins_path, 'clang')
            self._env['CXX'] = os.path.join(self._llvm_bins_path, 'clang++')
            self._env['LD'] = os.path.join(self._llvm_bins_path, 'clang')

        if sys.platform == 'darwin':
            # The system/xcode compiler would find system SDK correctly, but
            # the Clang we've built does not. See
            # https://github.com/llvm/llvm-project/issues/45225
            sdk_path = subprocess.check_output(['xcrun', '--show-sdk-path'],
                                               text=True).rstrip()
            self._env['CFLAGS'] += f' -isysroot {sdk_path}'
            self._env['CXXFLAGS'] += f' -isysroot {sdk_path}'
            self._env['LDFLAGS'] += f' -isysroot {sdk_path}'
            self._env['RUSTFLAGS_BOOTSTRAP'] += (
                f' -Clink-arg=-isysroot -Clink-arg={sdk_path}')
            self._env['RUSTFLAGS_NOT_BOOTSTRAP'] += (
                f' -Clink-arg=-isysroot -Clink-arg={sdk_path}')
            # Rust compiletests don't get any of the RUSTFLAGS that we set here
            # and then the clang linker can't find `-lSystem`, unless we set the
            # `SDKROOT`.
            self._env['SDKROOT'] = sdk_path

        if zlib_path:
            self._env['CFLAGS'] += f' -I{zlib_path}'
            self._env['CXXFLAGS'] += f' -I{zlib_path}'
            self._env['LDFLAGS'] += f' {LD_PATH_FLAG}{zlib_path}'
            self._env['RUSTFLAGS_BOOTSTRAP'] += (
                f' -Clink-arg={LD_PATH_FLAG}{zlib_path}')
            self._env['RUSTFLAGS_NOT_BOOTSTRAP'] += (
                f' -Clink-arg={LD_PATH_FLAG}{zlib_path}')

        if libxml2_dirs:
            self._env['CFLAGS'] += f' -I{libxml2_dirs.include_dir}'
            self._env['CXXFLAGS'] += f' -I{libxml2_dirs.include_dir}'
            self._env['LDFLAGS'] += f' {LD_PATH_FLAG}{libxml2_dirs.lib_dir}'
            self._env['RUSTFLAGS_BOOTSTRAP'] += (
                f' -Clink-arg={LD_PATH_FLAG}{libxml2_dirs.lib_dir}')
            self._env['RUSTFLAGS_NOT_BOOTSTRAP'] += (
                f' -Clink-arg={LD_PATH_FLAG}{libxml2_dirs.lib_dir}')

        if debian_sysroot:
            # This mainly influences the glibc version that rustc itself needs.
            sysroot_cflag = f'--sysroot={debian_sysroot}'

            self._env['CFLAGS'] += f' {sysroot_cflag}'
            self._env['CXXFLAGS'] += f' {sysroot_cflag}'
            self._env['LDFLAGS'] += f' {sysroot_cflag}'

            self._env['RUSTFLAGS'] += f' -Clink-arg={sysroot_cflag}'

            # pkg-config will by default look for system-wide libs. This tells
            # it to look exclusively in the sysroot instead.
            self._env['PKG_CONFIG_SYSROOT_DIR'] = debian_sysroot

            # Due to an interaction with the above flags, we must tell lzma-sys
            # explicitly to build it from source.
            self._env['LZMA_API_STATIC'] = '1'

        # TODO(danakj): On windows we point the to lld-link in config.toml so
        # we don't (and can't) specify -fuse-ld=lld, as lld-link doesn't know
        # that argument, and it's already using lld. Should we do the same for
        # other platforms and remove this link argument?
        if sys.platform != 'win32':
            # Direct rustc to use Chromium's lld instead of the system linker.
            # This is critical for stage 1 onward since we need to link libs
            # with LLVM bitcode. It is also good for a hermetic build in
            # general.
            self._env['RUSTFLAGS_BOOTSTRAP'] += ' -Clink-arg=-fuse-ld=lld'
            self._env['RUSTFLAGS_NOT_BOOTSTRAP'] += ' -Clink-arg=-fuse-ld=lld'

        # The `--undefined-version` flag is needed due to a bug in libtest:
        # https://github.com/rust-lang/rust/issues/105967. The flag does
        # not exist on Mac or Windows.
        if sys.platform.startswith('linux'):
            self._env['RUSTFLAGS_BOOTSTRAP'] += (
                ' -Clink-arg=-Wl,--undefined-version')
            self._env['RUSTFLAGS_NOT_BOOTSTRAP'] += (
                ' -Clink-arg=-Wl,--undefined-version')

        # Rustdoc should use our clang linker as well, as we pass flags that
        # the system linker may not understand.
        self._env['RUSTDOCFLAGS'] += f' -Clinker={self._env["LD"]}'

        # Cargo normally stores files in $HOME. Override this.
        self._env['CARGO_HOME'] = CARGO_HOME_DIR

        # https://crbug.com/1441182
        # Pretend we're a CI, since some path length reduction features are
        # only enabled for CI. Otherwise some tests fail when the rust src
        # directory name length is too long.
        self._env['GITHUB_ACTIONS'] = 'true'

    def configure(self, build_mac_arm, x86_64_llvm_config,
                  aarch64_llvm_config):
        # Read the config.toml template file...
        with open(RUST_CONFIG_TEMPLATE_PATH, 'r') as input:
            template = string.Template(input.read())

        def quote_string(s: str):
            return s.replace('\\', '\\\\').replace('"', '\\"')

        subs = {}
        subs['INSTALL_DIR'] = quote_string(str(RUST_TOOLCHAIN_OUT_DIR))
        subs['LLVM_BIN'] = quote_string(str(self._llvm_bins_path))
        subs['PACKAGE_VERSION'] = GetRustClangRevision()

        subs["LLVM_CONFIG_WINDOWS_x86_64"] = quote_string(
            str(x86_64_llvm_config))
        subs["LLVM_CONFIG_APPLE_AARCH64"] = quote_string(
            str(aarch64_llvm_config))
        subs["LLVM_CONFIG_APPLE_X86_64"] = quote_string(
            str(x86_64_llvm_config))
        subs["LLVM_CONFIG_LINUX_X86_64"] = quote_string(
            str(x86_64_llvm_config))

        # ...and apply substitutions, writing to config.toml in Rust tree.
        with open(os.path.join(RUST_SRC_DIR, 'config.toml'), 'w') as output:
            output.write(template.substitute(subs))

    def run(self, sub, args):
        ''' Run x.py subcommand with specified args. '''
        os.chdir(RUST_SRC_DIR)
        cmd = [sys.executable, 'x.py', sub]
        if self._verbose and self._verbose > 0:
            cmd.append('-' + self._verbose * 'v')
        RunCommand(cmd + args, setenv=True, env=self._env)
        os.chdir(CHROMIUM_DIR)

    def get_env(self):
        ''' The environment variables set for x.py invocations, as a dict. '''
        return self._env


# Get arguments to run desired test suites, minus disabled tests.
def GetTestArgs():
    args = TEST_SUITES
    for excluded in EXCLUDED_TESTS:
        args.append('--exclude')
        args.append(excluded)
    if sys.platform == 'win32':
        for excluded in EXCLUDED_TESTS_WINDOWS:
            args.append('--exclude')
            args.append(excluded)
    return args


def MakeVersionStamp(git_hash):
    # We must generate a version stamp that contains the full version of the
    # built Rust compiler:
    # * The version number returned from `rustc --version`.
    # * The git hash.
    # * The chromium revision name of the compiler build, which includes the
    #   associated clang/llvm version.
    with open(RUST_SRC_VERSION_FILE_PATH) as version_file:
        rust_version = version_file.readline().rstrip()
    return (f'rustc {rust_version} {git_hash}'
            f' ({GetRustClangRevision()} chromium)\n')


def GetLatestRustCommit():
    """Get the latest commit hash in the LLVM monorepo."""
    url = (
        'https://chromium.googlesource.com/external/' +
        'github.com/rust-lang/rust/+/refs/heads/master?format=JSON'  # nocheck
    )
    main = json.loads(
        urllib.request.urlopen(url).read().decode("utf-8").replace(")]}'", ""))
    return main['commit']


def RustTargetTriple(build_mac_arm=False):
    if sys.platform == 'darwin':
        if platform.machine() == 'arm64' or build_mac_arm:
            return 'aarch64-apple-darwin'
        else:
            return 'x86_64-apple-darwin'
    elif sys.platform == 'win32':
        return 'x86_64-pc-windows-msvc'
    else:
        return 'x86_64-unknown-linux-gnu'


# Fetch or build the LLVM libraries, for the host machine and when
# cross-compiling for the target machine.
#
# Returns the path to llvm-config for x86_64 and aarch64 targets.
def BuildLLVMLibraries(skip_build, build_mac_arm):
    # LLVM libraries for the target machine. Usually the same as the host,
    # unless we are cross-compiling.
    if build_mac_arm:
        target_llvm_build_dir = RUST_CROSS_TARGET_LLVM_BUILD_DIR
        target_llvm_install_dir = RUST_CROSS_TARGET_LLVM_INSTALL_DIR
    else:
        target_llvm_build_dir = RUST_HOST_LLVM_BUILD_DIR
        target_llvm_install_dir = RUST_HOST_LLVM_INSTALL_DIR

    if not skip_build:
        print(f'Building the host LLVM in {RUST_HOST_LLVM_BUILD_DIR}...')
        build_cmd = [
            sys.executable,
            os.path.join(CLANG_SCRIPTS_DIR, 'build.py'),
            '--disable-asserts',
            '--no-tools',
            # PIC needed for Rust build (links LLVM into shared object)
            '--pic',
            '--with-ml-inliner-model=',
        ]
        if sys.platform.startswith('linux'):
            build_cmd.append('--without-android')
            build_cmd.append('--without-fuchsia')
        if sys.platform == 'darwin':
            build_cmd.append('--without-fuchsia')
        RunCommand(build_cmd + [
            '--build-dir', RUST_HOST_LLVM_BUILD_DIR, '--install-dir',
            RUST_HOST_LLVM_INSTALL_DIR
        ])

        # Build target compiler.
        if build_mac_arm:
            print(f'Building the target cross-compile LLVM in '
                  f'{target_llvm_build_dir}...')
            build_cmd.append('--build-mac-arm')
            if not skip_build:
                RunCommand(build_cmd + [
                    '--build-dir', target_llvm_build_dir, '--install-dir',
                    target_llvm_install_dir
                ])

            print(f'Copying the target-but-native llvm-config to LLVM '
                  f'install dir...')
            shutil.copy(
                os.path.join(target_llvm_install_dir, 'bin', 'llvm-config'),
                os.path.join(target_llvm_install_dir, 'bin',
                             'llvm-config.bak'))
            shutil.copy(
                os.path.join(target_llvm_build_dir, 'NATIVE', 'bin',
                             'llvm-config'),
                os.path.join(target_llvm_install_dir, 'bin', 'llvm-config'))

    # Default to pointing all architectures at the host machine on the
    # assumption we are not cross-compiling.
    x86_64_llvm_config = os.path.join(RUST_HOST_LLVM_BUILD_DIR, 'bin',
                                      'llvm-config')
    aarch64_llvm_config = os.path.join(RUST_HOST_LLVM_BUILD_DIR, 'bin',
                                       'llvm-config')
    # Cross-compiling options follow.
    if build_mac_arm:
        # If we're building for aarch64 on an x86_64 machine, then we have a
        # separate `target_llvm_install_dir` which has the aarch64 things.

        # Inside a cross-compiled LLVM build, in the NATIVE directory, is an
        # llvm-config that can run on the host machine.
        aarch64_llvm_config = os.path.join(target_llvm_install_dir, 'bin',
                                           'llvm-config')
    return (x86_64_llvm_config, aarch64_llvm_config, target_llvm_install_dir)


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
    parser.add_argument(
        '--dump-env',
        action='store_true',
        help=
        'dump all environment variables set for x.py to a file `rust-build-env`'
    )
    parser.add_argument('--skip-checkout',
                        action='store_true',
                        help='do not create or update any checkouts')
    parser.add_argument('--sync-for-gnrt',
                        action='store_true',
                        help='sync checkout and deps for gnrt run then quit.')
    parser.add_argument('--skip-clean',
                        action='store_true',
                        help='skip x.py clean step')
    parser.add_argument(
        '--skip-llvm-build',
        action='store_true',
        help='do not build LLVM, presuming build_rust.py was '
        'already run and the LLVM libs are thus already present.')
    parser.add_argument('--skip-test',
                        action='store_true',
                        help='skip running rustc and libstd tests')
    parser.add_argument('--skip-install',
                        action='store_true',
                        help='do not install to RUST_TOOLCHAIN_OUT_DIR')
    parser.add_argument('--rust-force-head-revision',
                        action='store_true',
                        help='build the latest revision')
    parser.add_argument(
        '--prepare-run-xpy',
        action='store_true',
        help='set up the build directory to use --run-xpy subsequently. For '
        'debugging.')
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

    debian_sysroot = None
    if sys.platform.startswith('linux') and not args.sync_for_gnrt:
        # Fetch sysroot we build rustc against. This ensures a minimum supported
        # host (not Chromium target). Since the rustc linux package is for
        # x86_64 only, that is the sole needed sysroot.
        debian_sysroot = DownloadDebianSysroot('amd64')

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

    # TODO(crbug.com/1271215): OpenSSL is somehow already present on the Windows
    # builder, but we should change to using a package from 3pp when it is
    # available.
    if (sys.platform != 'win32' and not args.build_mac_arm
            and not args.sync_for_gnrt):
        # Building cargo depends on OpenSSL.
        AddOpenSSLToEnv(args.build_mac_arm)

    xpy = XPy(zlib_path, libxml2_dirs, args.build_mac_arm, debian_sysroot,
              args.verbose)

    if args.dump_env:
        with open('rust-build-env', 'w') as f:
            for name, val in xpy.get_env().items():
                print(f'{name}={val}', file=f)

    # Assume the checkout has already been prepared. A full build or a
    # --prepare-run-xpy run will set it up.
    if args.run_xpy:
        # Ensure the config.toml was previously generated.
        config_path = os.path.join(RUST_SRC_DIR, 'config.toml')
        assert os.path.exists(config_path)
        assert os.path.isfile(config_path)

        if rest[0] == '--':
            rest = rest[1:]
        xpy.run(rest[0], rest[1:])
        return 0
    else:
        assert not rest

    if sys.platform == 'win32':
        # Use curl to prime Windows's root cert store (crbug.com/1448442).
        RunCommand(['curl', '-I', 'https://static.rust-lang.org'])

    if args.rust_force_head_revision:
        assert not args.skip_checkout
        checkout_revision = GetLatestRustCommit()
    else:
        checkout_revision = RUST_REVISION

    if not args.skip_checkout:
        CheckoutGitRepo('Rust', RUST_GIT_URL, checkout_revision, RUST_SRC_DIR)

        path = FetchBetaPackage('cargo', checkout_revision)
        if sys.platform == 'win32':
            cargo_bin = os.path.join(path, 'cargo', 'bin', 'cargo.exe')
        else:
            cargo_bin = os.path.join(path, 'cargo', 'bin', 'cargo')
        CargoVendor(cargo_bin)

    # Gnrt needs the checkout to be up-to-date, workspace submodules to be
    # synced for cargo to work, and the cargo binary itself. All this is done,
    # so quit.
    if args.sync_for_gnrt:
        return 0

    VerifyStage0JsonHash()
    if args.verify_stage0_hash:
        # The above function exits and prints the actual hash if
        # verification failed so we just quit here; if we reach this point,
        # the hash is valid.
        return 0

    (x86_64_llvm_config, aarch64_llvm_config,
     target_llvm_dir) = BuildLLVMLibraries(args.skip_llvm_build,
                                           args.build_mac_arm)

    AddCMakeToPath()

    # Set up config.toml in Rust source tree.
    xpy.configure(args.build_mac_arm, x86_64_llvm_config, aarch64_llvm_config)

    # Deps are updated, so we're done now. All steps needed for --run-xpy to
    # work should be above this.
    if args.prepare_run_xpy:
        return 0

    building_on_host_triple = RustTargetTriple()
    xpy_args = ['--build', building_on_host_triple]
    if args.build_mac_arm:
        building_for_host_triple = RustTargetTriple(build_mac_arm=True)
        xpy_args.extend([
            # The compiler will run on ARM.
            '--host',
            building_for_host_triple,
            # The compiler will build stuff for ARM.
            '--target',
            building_for_host_triple
        ])

    if not args.skip_clean:
        print('Cleaning build artifacts...')
        xpy.run('clean', xpy_args)

    # When cross-compiling, the bootstrap does not reuse previous artifacts,
    # so by splitting the build up into steps, we end up building the compiler
    # multiple times. As such, when cross-compiling, we skip right to the
    # install step, which will build what is needed.
    if not args.build_mac_arm and not args.skip_install:
        if not args.skip_test and not args.build_mac_arm:
            print(f'Running stage 2 tests...')
            xpy.run('test', ['--stage', '2'] + xpy_args + GetTestArgs())

        # Build stage 2 compiler, tools, and libraries. This should reuse
        # earlier stages from the test command (if run).
        print('Building stage 2 artifacts...')
        xpy.run('build', xpy_args + ['--stage', '2'] + BUILD_TARGETS)

    if args.skip_install:
        # Rust is fully built. We can quit.
        return 0

    print(f'Installing Rust to {RUST_TOOLCHAIN_OUT_DIR} ...')
    # Clean output directory.
    if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
        RmTree(RUST_TOOLCHAIN_OUT_DIR)

    artifacts = DISTRIBUTION_ARTIFACTS
    if args.build_mac_arm:
        for a in DISTRIBUTION_ARTIFACTS_SKIPPED_CROSS_COMPILE:
            artifacts.remove(a)
    xpy.run('install', xpy_args + artifacts)

    # Copy additional vendored crates required for building stdlib.
    print(f'Copying vendored dependencies to {RUST_TOOLCHAIN_OUT_DIR} ...')
    shutil.copytree(RUST_SRC_VENDOR_DIR, RUST_TOOLCHAIN_SRC_DIST_VENDOR_DIR)

    with open(VERSION_SRC_PATH, 'w') as stamp:
        stamp.write(MakeVersionStamp(checkout_revision))

    return 0


if __name__ == '__main__':
    sys.exit(main())

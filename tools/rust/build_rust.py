#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Assembles a Rust toolchain in-tree linked against the LLVM revision
specified in //tools/clang/scripts/update.py.

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
builds LLVM and Clang via //tools/clang/scripts/build.py, then builds rustc and
libstd against it. Clang is included in the build for libclang, which is needed
for building and shipping bindgen.

Ideally our build would begin with our own trusted stage0 rustc. As it is
simpler, for now we use an official build.

TODO(crbug.com/40196262): Do a proper 3-stage build

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

from build import (AddCMakeToPath, AddZlibToPath, CheckoutGitRepo, CopyFile,
                   DownloadDebianSysroot, GetLibXml2Dirs, GitCherryPick,
                   LLVM_BUILD_TOOLS_DIR, RunCommand)
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
    # TODO(crbug.com/342026487): benign failure; remove when fixed.
    os.path.join('tests', 'codegen', 'vec-in-place.rs'),
    # TODO(crbug.com/360916952): Benign, remove when fixed.
    os.path.join('tests', 'assembly', 'x86_64-cmp.rs'),
    os.path.join('tests', 'assembly', 'x86_64-cmp.rs#OPTIM'),
    os.path.join('tests', 'codegen', 'integer-cmp.rs'),
    os.path.join('tests', 'codegen', 'comparison-operators-2-tuple.rs'),
]
EXCLUDED_TESTS_WINDOWS = [
    # https://github.com/rust-lang/rust/issues/96464
    os.path.join('tests', 'codegen', 'vec-shrink-panik.rs'),
]
EXCLUDED_TESTS_MAC = [
    # https://crbug.com/1521497 These fail on Mac.
    os.path.join('tests', 'ui', 'abi', 'stack-probes-lto.rs#x64'),
    os.path.join('tests', 'ui', 'abi', 'stack-probes.rs#x64'),
]
EXCLUDED_TESTS_MAC_ARM64 = [
    # https://crbug.com/1519640 This fails on Mac/ARM64. We didn't even run it
    # until recently, so ignore it for now.
    os.path.join('tests', 'ui', 'extern',
                 'issue-64655-extern-rust-must-allow-unwind.rs#fat0'),
    os.path.join('tests', 'ui', 'extern',
                 'issue-64655-extern-rust-must-allow-unwind.rs#thin0'),
]

CLANG_SCRIPTS_DIR = os.path.join(CHROMIUM_DIR, 'tools', 'clang', 'scripts')

RUST_GIT_URL = ('https://chromium.googlesource.com/external/' +
                'github.com/rust-lang/rust')

RUST_SRC_DIR = os.path.join(THIRD_PARTY_DIR, 'rust-src')
RUST_BUILD_DIR = os.path.join(RUST_SRC_DIR, 'build')
RUST_BOOTSTRAP_DIST_RS = os.path.join(RUST_SRC_DIR, 'src', 'bootstrap',
                                      'dist.rs')
STAGE0_JSON_PATH = os.path.join(RUST_SRC_DIR, 'src', 'stage0')
# Download crates.io dependencies to rust-src subdir (rather than $HOME/.cargo)
CARGO_HOME_DIR = os.path.join(RUST_SRC_DIR, 'cargo-home')
RUST_SRC_VERSION_FILE_PATH = os.path.join(RUST_SRC_DIR, 'src', 'version')
RUST_SRC_GIT_COMMIT_INFO_FILE_PATH = os.path.join(RUST_SRC_DIR,
                                                  'git-commit-info')
RUST_TOOLCHAIN_LIB_DIR = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'lib')
RUST_TOOLCHAIN_SRC_DIST_DIR = os.path.join(RUST_TOOLCHAIN_LIB_DIR, 'rustlib',
                                           'src', 'rust')
RUST_CONFIG_TEMPLATE_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), 'config.toml.template')
RUST_CARGO_CONFIG_TEMPLATE_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), 'cargo-config.toml.template')

RUST_HOST_LLVM_BUILD_DIR = os.path.join(CHROMIUM_DIR, 'third_party',
                                        'rust-toolchain-intermediate',
                                        'llvm-host-build')
RUST_HOST_LLVM_INSTALL_DIR = os.path.join(CHROMIUM_DIR, 'third_party',
                                          'rust-toolchain-intermediate',
                                          'llvm-host-install')

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
OPENSSL_CIPD_MAC_ARM_PATH = 'infra/3pp/static_libs/openssl/mac-arm64'
OPENSSL_CIPD_MAC_ARM_VERSION = '1.1.1j.chromium.2'
# TODO(crbug.com/40205621): Pull Windows OpenSSL from 3pp when it exists.

if sys.platform == 'win32':
    LD_PATH_FLAG = '/LIBPATH:'
else:
    LD_PATH_FLAG = '-L'

BUILD_TARGETS = [
    'library/proc_macro', 'library/std', 'src/tools/cargo', 'src/tools/clippy',
    'src/tools/rustfmt'
]

# Which test suites to run. Any failure will fail the build.
TEST_SUITES = [
    'library/std',
    'tests/codegen',
    'tests/ui',
]


def AddOpenSSLToEnv():
    """Download OpenSSL, and add to OPENSSL_DIR."""
    ssl_dir = os.path.join(LLVM_BUILD_TOOLS_DIR, 'openssl')

    if sys.platform == 'darwin':
        if platform.machine() == 'arm64':
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


def VerifyStage0JsonHash(stage0_json_url=None):
    hasher = hashlib.sha256()
    if stage0_json_url:
        print(stage0_json_url)
        base64_text = urllib.request.urlopen(stage0_json_url).read().decode(
            "utf-8")
        stage0 = base64.b64decode(base64_text)
        hasher.update(stage0)
    else:
        with open(STAGE0_JSON_PATH, 'rb') as input:
            hasher.update(input.read())

    actual_hash = hasher.hexdigest()
    if actual_hash == STAGE0_JSON_SHA256:
        return

    print('src/stage0 hash is different than expected!')
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

    # Pull the stage0 to find the package intended to be used to build this
    # version of the Rust compiler.
    STAGE0_JSON_URL = (
        'https://chromium.googlesource.com/external/github.com/'
        'rust-lang/rust/+/{GIT_HASH}/src/stage0?format=TEXT')
    base64_text = urllib.request.urlopen(
        STAGE0_JSON_URL.format(GIT_HASH=rust_git_hash)).read().decode("utf-8")
    stage0 = base64.b64decode(base64_text).decode("utf-8")
    lines = stage0.splitlines()

    # The stage0 file contains the path to all tarballs it uses binaries from.
    for l in lines:
        if l.startswith('dist_server='):
            server = l.split('=')[1]
        if (filename + '.tar.gz') in l:
            package_tgz = l.split('=')[0]

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


def VendorForStdlib(cargo_bin):
    '''Runs `cargo vendor` to pull down standard library dependencies.'''
    os.chdir(RUST_SRC_DIR)

    vendor_env = os.environ
    # The Cargo.toml files in the Rust toolchain may use nightly Cargo
    # features, but the cargo binary is beta. This env var enables the
    # beta cargo binary to allow nightly features anyway.
    # https://github.com/rust-lang/rust/commit/2e52f4deb0544480b6aefe2c0cc1e6f3c893b081
    vendor_env['RUSTC_BOOTSTRAP'] = '1'

    vendor_cmd = [
        cargo_bin, 'vendor', '--manifest-path', 'library/Cargo.toml',
        '--locked', '--versioned-dirs', 'library/vendor'
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

    def __init__(self, zlib_path, libxml2_dirs, debian_sysroot, verbose):
        self._debian_sysroot = debian_sysroot
        self._env = collections.defaultdict(str, os.environ)
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

            self._env['RUSTFLAGS_BOOTSTRAP'] += f' -Clink-arg={sysroot_cflag}'
            self._env[
                'RUSTFLAGS_NOT_BOOTSTRAP'] += f' -Clink-arg={sysroot_cflag}'

            # pkg-config will by default look for system-wide libs. This tells
            # it to look exclusively in the sysroot instead.
            self._env['PKG_CONFIG_SYSROOT_DIR'] = debian_sysroot
            self._env[
                'PKG_CONFIG_LIBDIR'] = debian_sysroot + '/usr/lib/pkgconfig'

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

    def configure(self):
        # Read the config.toml template file...
        with open(RUST_CONFIG_TEMPLATE_PATH, 'r') as input:
            template = string.Template(input.read())

        def quote_string(s: str):
            return s.replace('\\', '\\\\').replace('"', '\\"')

        subs = {}
        subs['INSTALL_DIR'] = quote_string(str(RUST_TOOLCHAIN_OUT_DIR))
        subs['LLVM_BIN'] = quote_string(str(self._llvm_bins_path))
        subs['PACKAGE_VERSION'] = GetRustClangRevision()

        # FIXME: Remove after next rust roll.
        if RUST_REVISION == 'ab71ee7a9214c2793108a41efb065aa77aeb7326':
            subs['CHANGELOG_SEEN'] = '''\
# Suppress x.py warning about configuration changes
changelog-seen = 2'''
        else:
            subs['CHANGELOG_SEEN'] = ''

        # ...and apply substitutions, writing to config.toml in Rust tree.
        with open(os.path.join(RUST_SRC_DIR, 'config.toml'), 'w') as output:
            output.write(template.substitute(subs))

        if self._debian_sysroot:
            # Similarly, generate a Cargo config.toml in CARGO_HOME_DIR.
            with open(RUST_CARGO_CONFIG_TEMPLATE_PATH, 'r') as input:
                template = string.Template(input.read())

            subs = {}
            subs['DEBIAN_SYSROOT'] = quote_string(str(self._debian_sysroot))

            if not os.path.exists(CARGO_HOME_DIR):
                os.makedirs(CARGO_HOME_DIR)
            with open(os.path.join(CARGO_HOME_DIR, 'config.toml'),
                      'w') as output:
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
    if sys.platform == 'darwin':
        for excluded in EXCLUDED_TESTS_MAC:
            args.append('--exclude')
            args.append(excluded)
    if sys.platform == 'darwin' and platform.machine() == 'arm64':
        for excluded in EXCLUDED_TESTS_MAC_ARM64:
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


def RustTargetTriple():
    if sys.platform == 'darwin':
        if platform.machine() == 'arm64':
            return 'aarch64-apple-darwin'
        else:
            return 'x86_64-apple-darwin'
    elif sys.platform == 'win32':
        return 'x86_64-pc-windows-msvc'
    else:
        return 'x86_64-unknown-linux-gnu'


# Build the LLVM libraries and install them .
def BuildLLVMLibraries(skip_build):
    if not skip_build:
        print(f'Building the host LLVM in {RUST_HOST_LLVM_BUILD_DIR}...')
        build_cmd = [
            sys.executable,
            os.path.join(CLANG_SCRIPTS_DIR, 'build.py'),
            '--disable-asserts',
            '--no-tools',
            '--no-runtimes',
            # PIC needed for Rust build (links LLVM into shared object)
            '--pic',
            '--with-ml-inliner-model=',
            # Not using this in Rust yet, see also crbug.com/1476464.
            '--without-zstd',
        ]
        if sys.platform.startswith('linux'):
            build_cmd.append('--without-android')
            build_cmd.append('--without-fuchsia')
        RunCommand(build_cmd + [
            '--build-dir', RUST_HOST_LLVM_BUILD_DIR, '--install-dir',
            RUST_HOST_LLVM_INSTALL_DIR
        ])


# Move a git submodule to point to a different branch.
#
# This is super non-trivial because the submodules are shallow, and thus
# also single-branch, and can not find any remote branch except the one
# they are attached to upstream. See this for the reasons and method to
# work around it:
# https://stackoverflow.com/questions/61483547/how-to-shallow-pull-submodule-that-is-tracked-by-branch-name
def GitMoveSubmoduleBranch(root_git, submodule, branch):
    print(f'Moving git submodule {submodule} to branch "{branch}"')

    os.chdir(RUST_SRC_DIR)
    # Point to the desired branch.
    RunCommand(
        ['git', 'submodule', 'set-branch', '--branch', branch, submodule])

    os.chdir(os.path.join(RUST_SRC_DIR, *submodule.split('/')))
    # Force the submodule update to fetch the branch we want, or it fails to
    # find the branch.
    RunCommand([
        'git', 'config', 'remote.origin.fetch',
        f'+refs/heads/{branch}:refs/remotes/origin/{branch}'
    ])

    os.chdir(RUST_SRC_DIR)
    # Fetch and checkout the branch.
    RunCommand(
        ['git', 'submodule', 'update', '--remote', '--depth', '1', submodule])
    RunCommand([
        'git', 'commit', '-m',
        f'Chromium: Moved submodule {submodule} to branch {branch}',
        '.gitmodules', submodule
    ],
               fail_hard=False)

    os.chdir(CHROMIUM_DIR)


def GitApplyCherryPicks():
    print('Applying cherry-picks...')

    ##### CHERRY PICKS HERE #####
    #
    # NOTE: Cherry-picks to submodules do not stick, because x.py will
    # update them which will clobber your changes. To apply cherry-picks to
    # a submodule we would need to patch RUST_SRC_DIR to point to a fork
    # of the submodule on a branch with the desired cherry-pick. For
    # example, with llvm-project, we could set up a fork of upstream and
    # cherry-pick fixes into it, then point RUST_SRC_DIR at that fork
    # with `GitMoveSubmoduleBranch()`.
    #############################

    # TODO Remove once LLVM rolls past llvmorg-20-init-3909-ge61d6066e267
    RunCommand([
        'git',
        '-C',
        RUST_SRC_DIR,
        'revert',
        '--no-edit',
        '-m',
        '1',
        '8c7a7e346be4cdf13e77ab4acbfb5ade819a4e60',
    ])

    # TODO(b/363219692): Remove once
    # https://github.com/rust-lang/rust/pull/129894 or a similar fix has been
    # merged.
    GitCherryPick(RUST_SRC_DIR, 'https://github.com/rust-lang/rust.git',
                  'f20103f9f3e35dad241dd81cd3ae9eb2dafb3f44')

    print('Finished applying cherry-picks.')


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
    if sys.platform == 'win32':
        parser.add_argument('--sh', help='path to the sh.exe to use')
    args, rest = parser.parse_known_args()

    if sys.platform == 'win32':
        if args.sh:
            assert args.sh.endswith('sh.exe')
            p = os.environ['PATH']
            shdir = os.path.dirname(args.sh)
            os.environ['PATH'] = f'{shdir};{p}'
        where = subprocess.check_output(['where.exe', 'sh'], text=True)
        if '\\gnubby\\' in where.splitlines()[0]:
            print("WARNING: It looks like you have gnubby sh.exe in your ")
            print(" PATH, but it does not support normalized paths of the ")
            print(" form `/c/foo` and will fail at the install step. Put the ")
            print(" sh.exe from the Git installation into your PATH first ")
            print(" when running this script or use --sh to specify the path ")
            print(" to sh.exe.")
            print("where sh.exe:")
            print(where)
            return 1

    debian_sysroot = None
    if sys.platform.startswith('linux') and not args.sync_for_gnrt:
        # Fetch sysroot we build rustc against. This ensures a minimum supported
        # host (not Chromium target). Since the rustc linux package is for
        # x86_64 only, that is the sole needed sysroot.
        debian_sysroot = DownloadDebianSysroot('amd64', args.skip_checkout)

    # Require zlib compression.
    if sys.platform == 'win32':
        zlib_path = AddZlibToPath(dry_run=args.skip_checkout)
    else:
        zlib_path = None

    # libxml2 is built when building LLVM, so we use that.
    if sys.platform == 'win32':
        libxml2_dirs = GetLibXml2Dirs()
    else:
        libxml2_dirs = None

    # TODO(crbug.com/40205621): OpenSSL is somehow already present on the
    # Windows builder, but we should change to using a package from 3pp when it
    # is available.
    if (sys.platform != 'win32' and not args.sync_for_gnrt):
        # Building cargo depends on OpenSSL.
        AddOpenSSLToEnv()

    xpy = XPy(zlib_path, libxml2_dirs, debian_sysroot, args.verbose)

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
        if args.verify_stage0_hash:
            VerifyStage0JsonHash(
                'https://chromium.googlesource.com/external/github.com/'
                'rust-lang/rust/+/{}/src/stage0?format=TEXT'.format(
                    checkout_revision))
            # The above function exits and prints the actual hash if
            # verification failed so we just quit here; if we reach this point,
            # the hash is valid.
            return 0
        CheckoutGitRepo('Rust', RUST_GIT_URL, checkout_revision, RUST_SRC_DIR)
        path = FetchBetaPackage('cargo', checkout_revision)
        if sys.platform == 'win32':
            cargo_bin = os.path.join(path, 'cargo', 'bin', 'cargo.exe')
        else:
            cargo_bin = os.path.join(path, 'cargo', 'bin', 'cargo')

        # Some Submodules are part of the workspace and need to exist (so we can
        # read their Cargo.toml files) before we can vendor their deps.
        submod_cmd = [
            'git', '-C', RUST_SRC_DIR, 'submodule', 'update', '--init',
            '--recursive', '--depth', '1'
        ]
        RunWithRetry(submod_cmd, 'git submodule')

        # This happens after initializing submodules, so that we can include
        # changes that move submodules.
        GitApplyCherryPicks()

        # TODO(crbug.com/356618943): Workaround for https://github.com/rust-lang/cargo/issues/14253
        bootstrap_cargo = os.path.join(RUST_SRC_DIR, 'src', 'bootstrap',
                                       'Cargo.toml')
        with open(bootstrap_cargo, 'r') as f:
            lines = f.readlines()
        with open(bootstrap_cargo, 'w') as f:
            for l in lines:
                if l.strip('\n') != 'debug = 0':
                    f.write(l)

        VendorForStdlib(cargo_bin)

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

    BuildLLVMLibraries(args.skip_llvm_build)

    AddCMakeToPath()

    # Set up config.toml in Rust source tree.
    xpy.configure()

    # Deps are updated, so we're done now. All steps needed for --run-xpy to
    # work should be above this.
    if args.prepare_run_xpy:
        return 0

    target_triple = RustTargetTriple()
    xpy_args = ['--build', target_triple]

    # Delete the build directory.
    if not args.skip_clean:
        print('Clearing build directory...')
        if os.path.exists(RUST_BUILD_DIR):
            RmTree(RUST_BUILD_DIR)

    if not args.skip_test:
        print(f'Building stage 2 artifacts and running tests...')
        xpy.run('test', ['--stage', '2'] + xpy_args + GetTestArgs())

    if not args.skip_install:
        # Build stage 2 compiler, tools, and libraries. This should reuse
        # earlier stages from the test command (if run).
        print('Installing stage 2 artifacts...')
        xpy.run('build', xpy_args + ['--stage', '2'] + BUILD_TARGETS)

    if args.skip_install:
        # Rust is fully built. We can quit.
        return 0

    print(f'Installing Rust to {RUST_TOOLCHAIN_OUT_DIR} ...')
    # Clean output directory.
    if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
        RmTree(RUST_TOOLCHAIN_OUT_DIR)

    xpy.run('install', xpy_args + [])

    # The Rust stdlib deps are vendored to rust-src/library/vendor, and later
    # the x.py install process copies all subdirs of rust-src/library to the
    # toolchain package, so we do not need to explicitly copy the vendor dir.
    # This is left as a note in case that behavior changes.

    with open(VERSION_SRC_PATH, 'w') as stamp:
        stamp.write(MakeVersionStamp(checkout_revision))

    return 0


if __name__ == '__main__':
    sys.exit(main())

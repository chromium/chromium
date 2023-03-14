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
                   GetLibXml2Dirs, LLVM_BUILD_TOOLS_DIR, RunCommand)
from update import (CLANG_REVISION, CLANG_SUB_REVISION, DownloadAndUnpack,
                    LLVM_BUILD_DIR, GetDefaultHostOs, RmTree, UpdatePackage)
import build

from update_rust import (CHROMIUM_DIR, RUST_REVISION, RUST_SUB_REVISION,
                         RUST_TOOLCHAIN_OUT_DIR, STAGE0_JSON_SHA256,
                         THIRD_PARTY_DIR, VERSION_STAMP_PATH,
                         GetPackageVersionForBuild)

EXCLUDED_TESTS = [
    # Temporarily disabled due to https://github.com/rust-lang/rust/issues/94322
    os.path.join('tests', 'ui', 'numeric', 'numeric-cast.rs'),
    # Temporarily disabled due to https://github.com/rust-lang/rust/issues/96497
    os.path.join('tests', 'codegen', 'issue-96497-slice-size-nowrap.rs'),
    # TODO(crbug.com/1347563): Re-enable when fixed.
    os.path.join('tests', 'codegen', 'sanitizer-cfi-emit-type-checks.rs'),
    os.path.join('tests', 'codegen',
                 'sanitizer-cfi-emit-type-metadata-itanium-cxx-abi.rs'),
    # Temporarily disabled due to https://github.com/rust-lang/rust/issues/45222
    # which appears to have regressed as of a recent LLVM update. This test is
    # purely performance related, not correctness.
    os.path.join('tests', 'codegen', 'issue-45222.rs')
]
EXCLUDED_TESTS_WINDOWS = [
    # https://github.com/rust-lang/rust/issues/96464
    os.path.join('tests', 'codegen', 'vec-shrink-panik.rs'),
]

RUST_GIT_URL = ('https://chromium.googlesource.com/external/' +
                'github.com/rust-lang/rust')

RUST_SRC_DIR = os.path.join(THIRD_PARTY_DIR, 'rust_src', 'src')
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

# Desired tools and libraries in our Rust toolchain.
DISTRIBUTION_ARTIFACTS = [
    'cargo', 'clippy', 'compiler/rustc', 'library/std', 'rust-analyzer',
    'rustfmt', 'src'
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


def Configure(llvm_bins_path, llvm_libs_root, link_tensorflow):
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

    if link_tensorflow:
        subs['RUSTC_LLVM_LDFLAGS'] = '-ltf_xla_runtime'
    else:
        subs['RUSTC_LLVM_LDFLAGS'] = ''

    # ...and apply substitutions, writing to config.toml in Rust tree.
    with open(os.path.join(RUST_SRC_DIR, 'config.toml'), 'w') as output:
        output.write(template.substitute(subs))


def FetchCargo(rust_git_hash):
    '''Downloads the beta Cargo package specified for the compiler build

    Unpacks the package and returns the path to the binary itself.
    '''
    if sys.platform == 'win32':
        target = 'cargo-beta-x86_64-pc-windows-msvc'
    elif sys.platform == 'darwin':
        if platform.machine() == 'arm64':
            target = 'cargo-beta-aarch64-apple-darwin'
        else:
            target = 'cargo-beta-x86_64-apple-darwin'
    else:
        target = 'cargo-beta-x86_64-unknown-linux-gnu'

    # Pull the stage0 JSON to find the Cargo binary intended to be used to
    # build this version of the Rust compiler.
    STAGE0_JSON_URL = (
        'https://chromium.googlesource.com/external/github.com/'
        'rust-lang/rust/+/{GIT_HASH}/src/stage0.json?format=TEXT')
    base64_text = urllib.request.urlopen(
        STAGE0_JSON_URL.format(GIT_HASH=rust_git_hash)).read().decode("utf-8")
    stage0 = json.loads(base64.b64decode(base64_text))

    # The stage0 JSON contains the path to all tarballs it uses binaries from,
    # including cargo.
    for k in stage0['checksums_sha256'].keys():
        if k.endswith(target + '.tar.gz'):
            cargo_tgz = k

    server = stage0['config']['dist_server']
    DownloadAndUnpack(f'{server}/{cargo_tgz}', LLVM_BUILD_TOOLS_DIR)

    bin_path = os.path.join(LLVM_BUILD_TOOLS_DIR, target, 'cargo', 'bin')
    if sys.platform == 'win32':
        return os.path.join(bin_path, 'cargo.exe')
    else:
        return os.path.join(bin_path, 'cargo')


def CargoVendor(cargo_bin):
    '''Runs `cargo vendor` to pull down dependencies.'''
    os.chdir(RUST_SRC_DIR)

    # Some Submodules are part of the workspace and need to exist (so we can
    # read their Cargo.toml files) before we can vendor their deps.
    RunCommand([
        'git', 'submodule', 'update', '--init', '--recursive', '--depth', '1'
    ])

    # From https://github.com/rust-lang/rust/blob/master/src/bootstrap/dist.rs#L986-L995
    # The additional `--sync` Cargo.toml files are not part of the top level
    # workspace.
    RunCommand([
        cargo_bin,
        'vendor',
        '--locked',
        '--versioned-dirs',
        '--sync',
        'src/tools/rust-analyzer/Cargo.toml',
        '--sync',
        'compiler/rustc_codegen_cranelift/Cargo.toml',
        '--sync',
        'src/bootstrap/Cargo.toml',
    ])

    # Make a `.cargo/config.toml` the points to the `vendor` directory for all
    # dependency crates.
    try:
        os.mkdir(os.path.join(RUST_SRC_DIR, '.cargo'))
    except FileExistsError:
        pass
    shutil.copyfile(RUST_CARGO_CONFIG_TEMPLATE_PATH,
                    os.path.join(RUST_SRC_DIR, '.cargo', 'config.toml'))


class XPy:
    ''' Runner for x.py, Rust's build script. Holds shared state between x.py
    runs. '''

    def __init__(self, llvm_bins_path, zlib_path, libxml2_dirs, build_mac_arm,
                 gcc_toolchain_path, verbose):
        self._env = collections.defaultdict(str, os.environ)
        self._build_mac_arm = build_mac_arm
        self._verbose = verbose

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
            self._env['AR'] = os.path.join(llvm_bins_path, 'llvm-lib.exe')
            self._env['CC'] = os.path.join(llvm_bins_path, 'clang-cl.exe')
            self._env['CXX'] = os.path.join(llvm_bins_path, 'clang-cl.exe')
            self._env['LD'] = os.path.join(llvm_bins_path, 'lld-link.exe')
        else:
            self._env['AR'] = os.path.join(llvm_bins_path, 'llvm-ar')
            self._env['CC'] = os.path.join(llvm_bins_path, 'clang')
            self._env['CXX'] = os.path.join(llvm_bins_path, 'clang++')
            self._env['LD'] = os.path.join(llvm_bins_path, 'clang')

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

        if gcc_toolchain_path:
            # We use these flags to avoid linking with the system libstdc++.
            gcc_toolchain_flag = f'--gcc-toolchain={gcc_toolchain_path}'
            self._env['CFLAGS'] += f' {gcc_toolchain_flag}'
            self._env['CXXFLAGS'] += f' {gcc_toolchain_flag}'
            self._env['LDFLAGS'] += f' {gcc_toolchain_flag}'
            # A `-Clink-arg=<foo>` arg passes `foo`` to the linker invovation.
            self._env['RUSTFLAGS_BOOTSTRAP'] += (
                f' -Clink-arg={gcc_toolchain_flag}')
            self._env['RUSTFLAGS_NOT_BOOTSTRAP'] += (
                f' -Clink-arg={gcc_toolchain_flag}')
            self._env['RUSTFLAGS_BOOTSTRAP'] += (
                f' -L native={gcc_toolchain_path}/lib64')
            self._env['RUSTFLAGS_NOT_BOOTSTRAP'] += (
                f' -L native={gcc_toolchain_path}/lib64')

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

    def run(self, sub, args):
        ''' Run x.py subcommand with specified args. '''
        # This enables the compiler to produce Mac ARM binaries.
        if self._build_mac_arm:
            args.extend(['--target', 'aarch64-apple-darwin'])

        os.chdir(RUST_SRC_DIR)
        cmd = [sys.executable, 'x.py', sub]
        if self._verbose and self._verbose > 0:
            cmd.append('-' + self._verbose * 'v')
        RunCommand(cmd + args, msvc_arch='x64', env=self._env)


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
            f' ({GetPackageVersionForBuild()} chromium)\n')


def GetLatestRustCommit():
    """Get the latest commit hash in the LLVM monorepo."""
    main = json.loads(
        urllib.request.urlopen('https://chromium.googlesource.com/external/' +
                               'github.com/rust-lang/rust/' +
                               '+/refs/heads/main?format=JSON').read().decode(
                                   "utf-8").replace(")]}'", ""))
    return main['commit']


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
    parser.add_argument('--skip-checkout',
                        action='store_true',
                        help='do not create or update any checkouts')
    parser.add_argument('--skip-clean',
                        action='store_true',
                        help='skip x.py clean step')
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
        '--fetch-llvm-libs',
        action='store_true',
        help='fetch Clang/LLVM libs and extract into LLVM_BUILD_DIR. Useless '
        'without --use-final-llvm-build-dir.')
    parser.add_argument(
        '--use-final-llvm-build-dir',
        action='store_true',
        help='use libs in LLVM_BUILD_DIR instead of LLVM_BOOTSTRAP_DIR. Useful '
        'with --fetch-llvm-libs for local builds. When enabled, these '
        'libraries must not use LTO.')
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

    # A production Rust toolchain will use our final-stage LLVM build. These
    # LLVM libs are built with LTO enabled, using the same LLVM revision from an
    # earlier build stage as backend. The Rust toolchain we start with (the
    # upstream beta release) however cannot process these since it uses an
    # earlier LLVM build as its backend.
    #
    # The Rust toolchain can be bootstrapped using earlier native-code LLVM
    # artifacts for its first stage, then using the final LLVM libs for further
    # stages. We do this because the bootstrap libs don't support targeting all
    # platforms, while the final LLVM libs do.
    #
    # Enable conditionally based on arguments and build host.
    #
    # TODO(https://crbug.com/1412187): hash out implementation issues on
    # non-Linux and enable unconditionally (sans script argument).
    use_lto_llvm = False
    if not args.use_final_llvm_build_dir and sys.platform.startswith('linux'):
        use_lto_llvm = True

    # Get the LLVM root for the stage0 build. This normally comes from the LLVM
    # bootstrap stage, but for local builds we support using LLVM_BUILD_DIR.
    if args.use_final_llvm_build_dir:
        llvm_bootstrap_root = LLVM_BUILD_DIR
    else:
        llvm_bootstrap_root = build.LLVM_BOOTSTRAP_DIR

    # If we're building for Mac ARM on an x86_64 Mac, we can't use the final
    # clang binaries as they don't have x86_64 support. Building them with that
    # support would blow up their size a lot. So, we use the bootstrap binaries
    # until such time as the Mac ARM builder becomes an actual Mac ARM machine.
    if not args.build_mac_arm:
        llvm_bins_path = os.path.join(LLVM_BUILD_DIR, 'bin')
    else:
        llvm_bins_path = os.path.join(build.LLVM_BOOTSTRAP_DIR, 'bin')

    if args.rust_force_head_revision:
        checkout_revision = GetLatestRustCommit()
    else:
        checkout_revision = RUST_REVISION

    if not args.skip_checkout:
        CheckoutGitRepo('Rust', RUST_GIT_URL, checkout_revision, RUST_SRC_DIR)

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

    # Set up config.toml in Rust source tree to configure build for stage0.
    # Normally, we will reconfigure later to use the production LLVM libs.
    Configure(llvm_bins_path, llvm_bootstrap_root, link_tensorflow=False)

    cargo_bin = FetchCargo(checkout_revision)
    CargoVendor(cargo_bin)

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

    # TODO(crbug.com/1271215): OpenSSL is somehow already present on the Windows
    # builder, but we should change to using a package from 3pp when it is
    # available.
    if sys.platform != 'win32':
        # Cargo depends on OpenSSL.
        AddOpenSSLToEnv(args.build_mac_arm)

    xpy = XPy(llvm_bins_path, zlib_path, libxml2_dirs, args.build_mac_arm,
              args.gcc_toolchain, args.verbose)

    if args.run_xpy:
        if rest[0] == '--':
            rest = rest[1:]
        xpy.run(rest[0], rest[1:])
        return 0
    else:
        assert not rest

    if not args.skip_clean:
        print('Cleaning build artifacts...')
        xpy.run('clean', [])

    # Run the stage0 build separately using the bootstrap LLVM libs. This will
    # allow further stages to process the LTO-enabled LLVM libs.
    #
    # On the other hand, when `use_lto_llvm` is disabled, we always use the
    # native-code bootstrap libs.
    #
    # The build has already been configured above for the bootstrap stage.
    xpy.run('build', ['--stage', '0', 'compiler/rustc'])

    xpy_args = []
    # Reconfigure to use production LLVM libs. After this point we must tell
    # x.py to keep the stage0 artifacts, even though config.toml changed.
    #
    # Note we must link tensorflow into rustc_llvm on Linux, since our LLVM
    # build includes it for MLGO. Unfortunately llvm-config does not report this
    # dependency. See https://github.com/llvm/llvm-project/issues/60751.
    if use_lto_llvm:
        Configure(llvm_bins_path,
                  LLVM_BUILD_DIR,
                  link_tensorflow=sys.platform.startswith('linux'))
        xpy_args = ['--keep-stage', '0']

    if not args.skip_test:
        print('Running stage 2 tests...')
        xpy.run('test', ['--stage', '2'] + xpy_args + GetTestArgs())

    targets = [
        'library/proc_macro', 'library/std', 'src/tools/cargo',
        'src/tools/clippy', 'src/tools/rustfmt'
    ]

    # Build stage 2 compiler, tools, and libraries. This should reuse earlier
    # stages from the test command (if run).
    print('Building stage 2 artifacts...')
    xpy.run('build', xpy_args + ['--stage', '2'] + targets)

    if args.skip_install:
        # Rust is fully built. We can quit.
        return 0

    print(f'Installing to {RUST_TOOLCHAIN_OUT_DIR} ...')
    # Clean output directory.
    if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
        shutil.rmtree(RUST_TOOLCHAIN_OUT_DIR)

    xpy.run('install', xpy_args + DISTRIBUTION_ARTIFACTS)

    # Copy additional sources required for building stdlib out of
    # RUST_TOOLCHAIN_SRC_DIST_DIR.
    print(f'Copying vendored dependencies to {RUST_TOOLCHAIN_OUT_DIR} ...')
    shutil.copytree(RUST_SRC_VENDOR_DIR, RUST_TOOLCHAIN_SRC_DIST_VENDOR_DIR)

    with open(VERSION_STAMP_PATH, 'w') as stamp:
        stamp.write(MakeVersionStamp(checkout_revision))

    return 0


if __name__ == '__main__':
    sys.exit(main())

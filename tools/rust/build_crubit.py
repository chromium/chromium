#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Builds the Crubit tools.

Builds the Crubit tools for generating Rust/C++ FFI bindings.

This script must be run after //tools/rust/build_rust.py as it uses the outputs
of that script in the compilation of Crubit. In particular it uses:
- The rust toolchain binaries and libraries in `RUST_TOOLCHAIN_OUT_DIR`.
- The LLVM and Clang libraries and headers in `RUST_HOST_LLVM_INSTALL_DIR`.
- The `//third_party/llvm` checkout that those libraries were built from (its
  `git-commit-info` file provides `CRUBIT_LLVM_DEV_DATE`).
- On Windows, the `zlib.lib` and `libxml2s.lib` that the same run produced.
- On Linux, the Debian sysroot that the same run downloaded.

This script:
- Clones the Crubit repository, checks out a defined revision.
- Builds external C++ dependencies (Abseil, Protobuf) using CMake.
- Builds Crubit's `cc_bindings_from_rs` and `rs_bindings_from_cc` using Cargo.
- Copies the built tools into `RUST_TOOLCHAIN_OUT_DIR`, along with Crubit's
  support library.
'''

import argparse
import contextlib
import os
import re
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
    DEFAULT_MACOSX_DEPLOYMENT_TARGET,
    GIT_COMMIT_INFO_FILENAME,
    LLVM_DIR,
    AddCMakeToPath,
    AddZlibToPath,
    CheckoutGitRepo,
    GetLatestCommit,
    GetLibXml2Dirs,
    GetThirdPartyCMakeArgs,
    RunCommand,
)
from update import DownloadAndUnpack, ReadStampFile, WriteStampFile

from build_rust import GetMacSdkPath, RUST_HOST_LLVM_INSTALL_DIR
from host_tools import (
    EXE,
    GetHostCargoEnv,
    GetHostCcTools,
    GetHostDebianSysroot,
    GetHostLinker,
    RunHostCargo,
)
from update_rust import CHROMIUM_DIR, CRUBIT_REVISION, RUST_TOOLCHAIN_OUT_DIR

########################################################################
# Constants.

CRUBIT_GIT = (
    'https://chromium.googlesource.com/external/github.com/google/crubit'
)

ABSEIL_TARBALL_URL_PREFIX = (
    'https://chromium.googlesource.com/external/'
    'github.com/abseil/abseil-cpp/+archive/refs/tags'
)

PROTOBUF_TARBALL_URL_PREFIX = (
    'https://chromium.googlesource.com/external/'
    'github.com/protocolbuffers/protobuf/+archive/refs/tags'
)

# Directory for sources that are checked out or downloaded by this script (and
# by `build_rust.py`) but are not part of the Chromium checkout.
RUST_TOOLCHAIN_INTERMEDIATE_DIR = os.path.join(
    CHROMIUM_DIR, 'third_party', 'rust-toolchain-intermediate'
)

CRUBIT_SRC_DIR = os.path.join(RUST_TOOLCHAIN_INTERMEDIATE_DIR, 'crubit')
ABSEIL_SRC_DIR = os.path.join(
    RUST_TOOLCHAIN_INTERMEDIATE_DIR, 'crubit-abseil-src'
)
PROTOBUF_SRC_DIR = os.path.join(
    RUST_TOOLCHAIN_INTERMEDIATE_DIR, 'crubit-protobuf-src'
)

# Set to `False` to skip `rs_bindings_from_cc` and its C++ dependencies
# (Abseil, Protobuf) if they break the toolchain build.  Nothing in Chromium
# uses `rs_bindings_from_cc` yet.  `cc_bindings_from_rs` is always built,
# because Chromium builds need it.
ENABLE_BUILDING_RS_BINDINGS_FROM_CC = True

# The Crubit binaries that this script builds and installs.  All are members
# of the root cargo workspace (see Crubit's `Cargo.toml`).
CRUBIT_BINS = ['cc_bindings_from_rs']
if ENABLE_BUILDING_RS_BINDINGS_FROM_CC:
    CRUBIT_BINS.append('rs_bindings_from_cc')

# One argument per binary that makes it start, print a short text, and exit
# with 0 (see `SmokeTestCrubit`).
SMOKE_TEST_ARGS = {
    'cc_bindings_from_rs': '--help',
    'rs_bindings_from_cc': '--version',
}

# Abseil and Protobuf versions must be kept in sync with the versions that
# Crubit pins via `bazel_dep(...)` in its `MODULE.bazel`:
# https://github.com/google/crubit/blob/main/MODULE.bazel
# `CheckDependencyVersion` below enforces this at build time.
ABSEIL_VERSION = '20260526.0'
PROTOBUF_VERSION = '36.1'

# Name of the stamp file in each CMake install dir (see `BuildCMakeProject`).
CMAKE_STAMP_FILENAME = 'build_crubit_stamp'

########################################################################
# Platform helpers.

IS_WIN = sys.platform == 'win32'
IS_MAC = sys.platform == 'darwin'
IS_LINUX = sys.platform.startswith('linux')


def GetCrubitCxxFlags():
    """Returns C++ flags for all C++ code in the Crubit binaries.

    Used for both the CMake-built dependencies and the cargo-built C++ code.
    """
    if IS_MAC:
        # Aligned `new` needs macOS 10.13+, but the deployment target is
        # older (`DEFAULT_MACOSX_DEPLOYMENT_TARGET`).  Inert on arm64.
        return ['-faligned-allocation']
    return []


########################################################################
# Crubit repository helpers.


def GetLatestCrubitCommit():
    """Get the latest commit hash in the Crubit repo."""
    url = CRUBIT_GIT + '/+/refs/heads/upstream/main?format=JSON'
    return GetLatestCommit(url)


def CheckDependencyVersion(module_name, pinned_version):
    """Fails if `pinned_version` disagrees with Crubit's `MODULE.bazel`.

    Bazel Central Registry version strings live in BCR's own namespace: a BCR
    re-release of the same upstream version adds a `.bcr.<N>` suffix (for
    example protobuf's `36.1.bcr.1` for upstream `36.1`).  That suffix is
    stripped before comparing; everything else must match exactly.

    Matching version doesn't necessarily indicate identical content (because BCR
    can carry additional patches), but in practice the only differences are some
    minor Bazel tweaks which don't apply to the CMake build we are driving here.
    """
    module_bazel = os.path.join(CRUBIT_SRC_DIR, 'MODULE.bazel')
    with open(module_bazel, encoding='utf-8') as f:
        content = f.read()
    match = re.search(
        rf'bazel_dep\(\s*name\s*=\s*"{re.escape(module_name)}"\s*,'
        rf'\s*version\s*=\s*"([^"]+)"',
        content,
    )
    # A toolchain gardener may see the errors below first, so point at the
    # off switch too.
    workaround = (
        f'  To unblock the toolchain build, you can set '
        f'`ENABLE_BUILDING_RS_BINDINGS_FROM_CC = False` in {__file__}.'
    )
    if not match:
        raise RuntimeError(
            f'Could not find `bazel_dep(name = "{module_name}", ...)` in '
            f'{module_bazel}.  Crubit may have changed how it pins this '
            f'dependency - update {__file__} accordingly.' + workaround
        )
    bcr_version = match.group(1)
    upstream_version = re.sub(r'\.bcr\.\d+$', '', bcr_version)
    if upstream_version != pinned_version:
        raise RuntimeError(
            f'{module_name}: Crubit pins {bcr_version} in {module_bazel}, but '
            f'this script builds {pinned_version}.  Update the version '
            f'constant (and, if BCR changed its version format, the comparison '
            f'in `CheckDependencyVersion`).' + workaround
        )


########################################################################
# CMake helpers.


def GetCMakePath(path):
    """Returns `path` as an absolute path, with forward slashes on Windows.

    CMake treats backslashes in command-line definitions as escape characters,
    so paths on Windows must use forward slashes.
    """
    abs_path = os.path.abspath(path)
    if IS_WIN:
        abs_path = abs_path.replace('\\', '/')
    return abs_path


def CreateCMakePathFlag(var_name, path):
    """Formats a -DVAR=PATH CMake argument (see `GetCMakePath`)."""
    return f'-D{var_name}={GetCMakePath(path)}'


def BuildCMakeProject(
    name,
    url,
    src_dir,
    build_dir,
    install_dir,
    extra_args,
    extra_stamp_lines=(),
):
    """Downloads, builds, and installs a CMake project, unless up to date.

    The sources are downloaded from `url` into `src_dir` only when the project
    has to be (re)built.

    `install_dir` holds a stamp file with `name` (which includes the version),
    `extra_stamp_lines`, and all the CMake arguments.  If the stamp is
    different, then the project is rebuilt from scratch.  So a version bump or
    a flag change takes effect even when `--out-dir` is reused.  The old
    `install_dir` is removed, because Crubit's cargo build links all the
    Abseil libraries that it finds there.

    Returns the stamp, so that dependent projects can add it to their stamp.
    """
    cc_bin, cxx_bin, _ = GetHostCcTools()
    cmake_args = GetThirdPartyCMakeArgs(
        GetCMakePath(cc_bin),
        GetCMakePath(cxx_bin),
        GetHostDebianSysroot() if IS_LINUX else None,
        DEFAULT_MACOSX_DEPLOYMENT_TARGET,
    ) + [
        CreateCMakePathFlag('CMAKE_INSTALL_PREFIX', install_dir),
        '-DCMAKE_CXX_STANDARD=20',
        '-DCMAKE_CXX_STANDARD_REQUIRED=ON',
        '-DBUILD_SHARED_LIBS=OFF',
    ]
    # Link `protoc` with `lld` rather than with the host's linker (this mimics
    # what `build.py` does).
    #
    # TODO(https://crbug.com/562142145): Using `CMAKE_LINKER_TYPE` may be
    # cleaner, but requires CMake 3.29.
    if IS_WIN:
        # On Windows, CMake calls the linker directly.
        cmake_args.append(CreateCMakePathFlag('CMAKE_LINKER', GetHostLinker()))
    else:
        cmake_args.append('-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld')
    cxxflags = GetCrubitCxxFlags()
    if cxxflags:
        # Not passed when empty, because that would also remove CMake's
        # platform defaults (e.g. `/EHsc` on Windows).
        cmake_args.append('-DCMAKE_CXX_FLAGS=' + ' '.join(cxxflags))
    cmake_args += extra_args

    stamp = '\n'.join([name, *extra_stamp_lines, *cmake_args])
    stamp_file = os.path.join(install_dir, CMAKE_STAMP_FILENAME)
    if ReadStampFile(stamp_file) == stamp:
        print(f'{name} is up to date, skipping.')
        return stamp

    print(f'Downloading {name} ...')
    for d in [src_dir, build_dir, install_dir]:
        if os.path.exists(d):
            shutil.rmtree(d)
    DownloadAndUnpack(url, src_dir)
    print(f'Downloading {name} ... done.')

    print(f'Building {name} ...')
    os.makedirs(build_dir)

    # CMake uses `CFLAGS`, `CXXFLAGS`, and `LDFLAGS` to initialize the
    # `CMAKE_<LANG>_FLAGS` that it doesn't get on the command line.  Remove
    # them, so that the build doesn't depend on the environment.
    env = {
        k: v
        for k, v in os.environ.items()
        if k not in ('CFLAGS', 'CXXFLAGS', 'LDFLAGS')
    }
    abs_src_dir = os.path.abspath(src_dir)
    old_cwd = os.getcwd()
    try:
        os.chdir(build_dir)
        RunCommand(
            ['cmake', '-GNinja'] + cmake_args + [abs_src_dir],
            setenv=True,
            env=env,
        )
        RunCommand(['ninja', 'install'], setenv=True, env=env)
    finally:
        os.chdir(old_cwd)

    WriteStampFile(stamp, stamp_file)
    print(f'Building {name} ... done.')
    return stamp


########################################################################
# Abseil and Protobuf.


def BuildAbseil(build_dir, install_dir):
    """Returns the stamp of the Abseil build (see `BuildCMakeProject`)."""
    # Abseil overwrites `CMAKE_MSVC_RUNTIME_LIBRARY` (from
    # `GetThirdPartyCMakeArgs`), so without this it selects the DLL CRT on
    # Windows.
    extra_args = ['-DABSL_MSVC_STATIC_RUNTIME=ON'] if IS_WIN else []
    return BuildCMakeProject(
        f'Abseil {ABSEIL_VERSION}',
        f'{ABSEIL_TARBALL_URL_PREFIX}/{ABSEIL_VERSION}.tar.gz',
        ABSEIL_SRC_DIR,
        build_dir,
        install_dir,
        extra_args,
    )


def BuildProtobuf(abseil_install_dir, abseil_stamp, build_dir, install_dir):
    """Builds Protobuf against the Abseil in `abseil_install_dir`."""
    # Point Protobuf directly at our Abseil package.  CMake ignores an
    # `absl_DIR` without `abslConfig.cmake` and searches the system instead,
    # so check that the file is there.
    absl_dir = os.path.join(abseil_install_dir, 'lib', 'cmake', 'absl')
    if not os.path.exists(os.path.join(absl_dir, 'abslConfig.cmake')):
        raise RuntimeError(f'abslConfig.cmake not found in {absl_dir}')

    extra_args = [
        # This is the default, but older Protobuf releases defaulted to ON,
        # and the tests need GoogleTest, which this build doesn't provide.
        '-Dprotobuf_BUILD_TESTS=OFF',
        '-Dprotobuf_LOCAL_DEPENDENCIES_ONLY=ON',
        # Without this, CMake picks up the host's zlib (if any) for Protobuf's
        # gzip streams, which Crubit doesn't use.  This keeps the build
        # hermetic.
        '-Dprotobuf_WITH_ZLIB=OFF',
        CreateCMakePathFlag('absl_DIR', absl_dir),
    ]
    # `abseil_stamp` makes sure that Protobuf is rebuilt when the version or
    # the flags of Abseil change.
    BuildCMakeProject(
        f'Protobuf {PROTOBUF_VERSION}',
        f'{PROTOBUF_TARBALL_URL_PREFIX}/v{PROTOBUF_VERSION}.tar.gz',
        PROTOBUF_SRC_DIR,
        build_dir,
        install_dir,
        extra_args,
        extra_stamp_lines=[abseil_stamp],
    )


########################################################################
# Rustflags.


def GetRustcDriverRpathFlags():
    """Returns rustflags that help `cc_bindings_from_rs` find `rustc_driver`.

    We need to help the runtime linker find the path to
    `librustc_driver-xxxxxxxxxxxxxxxx.so`.  This mimics how `rustc` is built
    as seen in
    https://github.com/rust-lang/rust/blob/b889870082dd0b0e3594bbfbebb4545d54710829/src/bootstrap/src/core/builder/cargo.rs#L285-L306
    See also https://crbug.com/460482110#comment14 - #comment16

    `rs_bindings_from_cc` does not link `rustc_driver`, so for that binary
    these flags are inert.
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


def GetCxxRuntimeRustFlags():
    """Returns rustflags that link the C++ standard library.

    `rs_bindings_from_cc` has C++ code, and it links the static Clang libraries
    from `RUST_HOST_LLVM_INSTALL_DIR`.  Note that Chromium's host LLVM is built
    against libstdc++ on Linux, unlike Crubit's OSS build, which uses libc++.
    `cc_bindings_from_rs` has no C++ code, so these flags are inert for it.

    `-stdlib=` is not needed here: `rustc` passes `-nodefaultlibs` to the
    linker driver, so the driver adds no C++ library.
    """
    if IS_LINUX:
        # Link libstdc++ statically, like `rustc` (`static-libstdcpp` in
        # `config.toml.template`) and Clang (`LLVM_STATIC_LINK_CXX_STDLIB` in
        # `build.py`).  `-static-libstdc++` would have no effect, because of
        # `-nodefaultlibs`.  `SetUpCargoEnv` stops the `cc` crate from linking
        # the shared libstdc++ (via `CXXSTDLIB`).
        return ['-Clink-arg=-l:libstdc++.a']
    if IS_MAC:
        # The system libc++ (the macOS SDK has no static libc++), like
        # `rustc` and Clang.
        return ['-Clink-arg=-lc++']
    # On Windows, the objects built with `/MT` already ask for the static
    # MSVC STL (`/DEFAULTLIB`), and `lld-link` finds it through `LIB`.
    return []


def GetNativeLibsRustFlags():
    """Returns rustflags needed to link native libs on Windows.

    See https://crbug.com/481661885 to learn why adding `zlib.lib` and
    `libxml2s.lib` paths is required to build Crubit binaries on Windows when
    using a Chromium-built Rust sysroot.

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

    `cc_bindings_from_rs` and `rs_bindings_from_cc` are members of the same
    cargo workspace and share many dependencies (clap, syn, quote, ...).  Cargo
    keys its fingerprints on `RUSTFLAGS`, so building the two with different
    flags would compile every shared dependency twice.  We therefore use one
    flag set for both, which also lets a single `cargo build` produce both
    binaries.  Each group of flags is needed by both binaries, or is inert for
    one of them.
    """
    return (
        GetRustcDriverRpathFlags()
        + GetCxxRuntimeRustFlags()
        + GetNativeLibsRustFlags()
    )


########################################################################
# Build environment.


def GetLlvmDevDate():
    """Returns the UTC commit date (YYYYMMDD) of the LLVM used to build Crubit.

    Crubit's C++ code needs `-DCRUBIT_LLVM_DEV_DATE` to pick the right Clang
    APIs (see `rs_bindings_from_cc/clang_compat_macros.h`).

    The date describes the `//third_party/llvm` checkout - the same sources
    that `build_rust.py` used to build the Clang libraries in
    `RUST_HOST_LLVM_INSTALL_DIR`.  Note that this is deliberately *not*
    `CLANG_REVISION`: `build_rust.py --llvm-force-head-revision` leaves the
    checkout at a newer commit, and Crubit must agree with the headers that
    are actually in use.

    We read the date from the file that `//tools/clang/scripts/build.py`
    writes when it checks LLVM out, rather than from git or from a Gitiles
    query.  The checkout has no `.git` directory when it comes from a source
    tarball, and such builds may have no network access either.
    """
    info_file = os.path.join(LLVM_DIR, GIT_COMMIT_INFO_FILENAME)
    try:
        with open(info_file, encoding='utf-8') as f:
            # Line 3 is the UTC commit date in `YYYY-MM-DD` format.
            date = f.read().splitlines()[2]
    except (IndexError, OSError) as e:
        raise RuntimeError(
            f'Cannot read the LLVM commit date from {info_file}: {e}.  The '
            'file is written by `tools/clang/scripts/build.py` when it checks '
            'LLVM out, so re-run that script (without `--skip-checkout`) to '
            'create it.'
        ) from e
    if not re.fullmatch(r'\d{4}-\d{2}-\d{2}', date):
        raise RuntimeError(f'{info_file} has a malformed commit date: {date}')
    return date.replace('-', '')


def SetUpBuildTools():
    """Puts the hermetic build tools and SDK settings into the environment."""
    # Ensure hermetic CMake and Ninja are in PATH.
    AddCMakeToPath()
    ninja_dir = os.path.join(CHROMIUM_DIR, 'third_party', 'ninja')
    os.environ['PATH'] = ninja_dir + os.pathsep + os.environ.get('PATH', '')

    if IS_MAC:
        os.environ['SDKROOT'] = GetMacSdkPath()
        os.environ['MACOSX_DEPLOYMENT_TARGET'] = (
            DEFAULT_MACOSX_DEPLOYMENT_TARGET
        )


########################################################################
# Building Crubit.


def BuildCppDependencies(out_dir):
    """Builds Abseil and Protobuf with CMake.

    Returns their install directories as an (abseil, protobuf) pair.
    """
    CheckDependencyVersion('abseil-cpp', ABSEIL_VERSION)
    CheckDependencyVersion('protobuf', PROTOBUF_VERSION)

    abseil_install_dir = os.path.join(out_dir, 'abseil-install')
    abseil_stamp = BuildAbseil(
        os.path.join(out_dir, 'abseil-build'),
        abseil_install_dir,
    )

    # Protobuf depends on Abseil, so it has to be built second.
    protobuf_install_dir = os.path.join(out_dir, 'protobuf-install')
    BuildProtobuf(
        abseil_install_dir,
        abseil_stamp,
        os.path.join(out_dir, 'protobuf-build'),
        protobuf_install_dir,
    )

    return abseil_install_dir, protobuf_install_dir


def SetUpCargoEnv(env, abseil_install_dir, protobuf_install_dir, out_dir):
    """Adds to `env` the settings that Crubit's cargo build scripts read.

    `env` comes from `GetHostCargoEnv`, which already makes the `cc` crate
    build the C++ parts of Crubit with the same compiler (`CC`/`CXX`/`AR`) and
    sysroot (`CFLAGS`/`CXXFLAGS`) that built the Clang libraries.

    The variable names below are Crubit's contract, not ours: they are read by
    `cargo/build/*.rs` in the Crubit checkout, and documented at
    https://crubit.rs/overview/cargo_build.html#rs_bindings_from_cc

    Returns the (protoc path, pre-generated proto header dir) pair.
    """
    # Where to find the Clang libraries that `rs_bindings_from_cc` links.
    env['CLANG_INCLUDE_PATH'] = os.path.join(
        RUST_HOST_LLVM_INSTALL_DIR, 'include'
    )
    env['CLANG_LIB_STATIC_PATH'] = os.path.join(
        RUST_HOST_LLVM_INSTALL_DIR, 'lib'
    )

    env['ABSL_INCLUDE_PATH'] = os.path.join(abseil_install_dir, 'include')
    env['ABSL_LIB_STATIC_PATH'] = os.path.join(abseil_install_dir, 'lib')

    # Protobuf C++ headers are pre-generated into `proto_gen_dir` rather than
    # by a build script, so they are just another include dir.
    proto_gen_dir = os.path.join(out_dir, 'crubit-proto-headers')
    os.makedirs(proto_gen_dir, exist_ok=True)
    env['PROTOBUF_INCLUDE_PATH'] = os.pathsep.join(
        [os.path.join(protobuf_install_dir, 'include'), proto_gen_dir]
    )
    env['PROTOBUF_LIB_STATIC_PATH'] = os.path.join(protobuf_install_dir, 'lib')

    protoc_path = os.path.join(protobuf_install_dir, 'bin', 'protoc' + EXE)
    env['PROTOC'] = protoc_path

    if IS_LINUX:
        # Without this, the `cc` crate links the shared libstdc++.
        # `GetCxxRuntimeRustFlags` links the static one instead.
        env['CXXSTDLIB'] = ''

    llvm_dev_date = GetLlvmDevDate()
    print(f'Using CRUBIT_LLVM_DEV_DATE={llvm_dev_date}')
    cxxflags = [f'-DCRUBIT_LLVM_DEV_DATE={llvm_dev_date}']
    cxxflags += GetCrubitCxxFlags()
    if IS_WIN:
        # The Clang and LLVM libraries are static, so their headers must not
        # declare the APIs as `__declspec(dllimport)`.  The Clang headers do
        # that unless `CLANG_BUILD_STATIC` is defined.  The LLVM headers do
        # that only if `llvm-config.h` defines
        # `LLVM_ENABLE_LLVM_EXPORT_ANNOTATIONS` (the Windows install does not
        # today) and `LLVM_BUILD_STATIC` is not defined.
        cxxflags += ['-DCLANG_BUILD_STATIC', '-DLLVM_BUILD_STATIC']
    env['CXXFLAGS'] += ' ' + ' '.join(cxxflags)

    return protoc_path, proto_gen_dir


def GenerateProtoHeaders(protoc_path, proto_gen_dir):
    """Pre-generates the Protobuf C++ headers that Crubit needs.

    This is a correctness fix, not an optimization: many of Crubit's C++
    support crates include `rs_bindings_from_cc/ir.pb.h`, and if their cargo
    build scripts generated it into a shared directory, the parallel `protoc`
    runs would race and the build would fail intermittently.  Generating the
    headers once, up front, removes the race.  Crubit documents the split in
    `docs/overview/cargo_build_protobuf.md`.
    """
    print('Generating Protobuf C++ headers for Crubit ...')
    generate_proto_script = os.path.join(
        CRUBIT_SRC_DIR, 'cargo', 'build', 'generate_proto_headers.py'
    )
    # `--protoc` and `--repo_root` have defaults (`$PROTOC` and the script's
    # own repo root) that happen to match what we want, but pass them anyway
    # so that this script does not depend on Crubit's choice of defaults.
    RunCommand(
        [
            sys.executable,
            generate_proto_script,
            f'--protoc={protoc_path}',
            f'--out_dir={proto_gen_dir}',
            f'--repo_root={CRUBIT_SRC_DIR}',
        ],
        setenv=True,
    )


def BuildCrubitBinaries(env, target_dir):
    """Builds all of `CRUBIT_BINS`; exits if `cargo` fails.

    `env` comes from `GetHostCargoEnv` (and `SetUpCargoEnv`).
    """
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
    env['RUSTFLAGS'] += ' ' + ' '.join(GetCrubitRustFlags())
    RunHostCargo(cargo_args, env)
    print(f'Building {bins} ... done.')


def InstallCrubit(release_dir):
    """Copies the built binaries and Crubit's support library into place."""
    print(f'Installing Crubit to {RUST_TOOLCHAIN_OUT_DIR} ...')
    os.makedirs(os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'bin'), exist_ok=True)
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

    if ENABLE_BUILDING_RS_BINDINGS_FROM_CC:
        SetUpBuildTools()
        abseil_install_dir, protobuf_install_dir = BuildCppDependencies(out_dir)

    # `GetHostCargoEnv` copies `os.environ`, so call it after `SetUpBuildTools`.
    env = GetHostCargoEnv(cargo_home=home_dir)
    if ENABLE_BUILDING_RS_BINDINGS_FROM_CC:
        protoc_path, proto_gen_dir = SetUpCargoEnv(
            env, abseil_install_dir, protobuf_install_dir, out_dir
        )
        GenerateProtoHeaders(protoc_path, proto_gen_dir)

    BuildCrubitBinaries(env, target_dir)
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

#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Run cargo from the chromium Rust toolchain.

Arguments are passed through to cargo.

Should be run from the checkout root (i.e. as `tools/crates/run_cargo.py ...`)
'''

import argparse
import os
import platform
import subprocess
import sys
import pathlib

DEFAULT_SYSROOT = pathlib.Path(__file__).parents[2].joinpath(
    'third_party', 'rust-toolchain')

# Determine the cargo executable name based on whether `subprocess` thinks
# we're on a Windows platform or not, which is more accurate than checking it
# ourselves. See https://bugs.python.org/issue8110 for more details.
_IS_WIN = 'STARTUPINFO' in subprocess.__all__
_CARGO_EXE = 'cargo.exe' if _IS_WIN else 'cargo'


# On windows, we need to point cargo and rustc to our hermetic windows
# toolchain, rather than assuming the windows tools are installed elsewhere
# on the system.
# This returns a string of flags separated by the 0x1f character, for use in
# the CARGO_ENCODED_RUSTFLAGS environment variable
def _GetWindowsToolchainFlags():
    # Get VS toolchain helpers from //build/vs_toolchain.py
    sys.path.append(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..',
                     'build'))

    from vs_toolchain import FindVCComponentRoot, SetEnvironmentAndGetSDKDir, SDK_VERSION

    # If DEPOT_TOOLS_WIN_TOOLCHAINset to 0, the user has explicitly requested
    # that we use their toolchain installation instead of the hermetic one
    if (not _IS_WIN or os.environ.get('DEPOT_TOOLS_WIN_TOOLCHAIN', '1') == '0'):
        return ""

    # The vs_toolchain function calls mess with the environment, so save a
    # backup to be restored after we're done.
    environ_backup = dict(os.environ)

    # Explicitly point to the linker executable

    # The paths for other architectures aren't clear, we can figure
    # them out if we need to.
    if (sys.maxsize <= 2**32 or sys.maxsize > 2**64
            or 'arm' in platform.machine().lower()):
        print("WARNING: run_cargo.py: non-x64 Windows host is not supported.\n",
              "This may fail in unexpected ways.")

    short_arch_path = 'x64'
    bin_arch_path = os.path.join('Hostx64', 'x64')

    linker_path = os.path.join(FindVCComponentRoot('Tools'), 'bin',
                               bin_arch_path, 'link.exe')
    include_paths = [
        os.path.join(FindVCComponentRoot('Tools'), 'lib', short_arch_path),
        os.path.join(SetEnvironmentAndGetSDKDir(), 'Lib', SDK_VERSION, 'um',
                     short_arch_path),
        os.path.join(SetEnvironmentAndGetSDKDir(), 'Lib', SDK_VERSION, 'ucrt',
                     short_arch_path),
    ]

    flags = [f'-Clinker={linker_path}'
             ] + [f"-Lnative={path}" for path in include_paths]

    os.environ.clear()
    os.environ.update(environ_backup)
    return chr(0x1f).join(flags)


def _PrependOrInsert(dict, key, sep, value):
    """Insert `value` into `dict[key]` if it doesn't already exist,
       otherwise prepend `sep + value` to the existing entry
    """
    if key in dict:
        dict[key] = value + sep + dict[key]
    else:
        dict[key] = value

def RunCargo(rust_sysroot, home_dir, cargo_args):
    rust_sysroot = pathlib.Path(rust_sysroot)
    if not rust_sysroot.exists():
        print(f'WARNING: Rust sysroot missing at "{rust_sysroot}"')

    bin_dir = rust_sysroot.absolute().joinpath('bin')
    cargo_path = bin_dir.joinpath(_CARGO_EXE)

    cargo_env = dict(os.environ)
    if home_dir:
        cargo_env['CARGO_HOME'] = home_dir

    _PrependOrInsert(cargo_env, 'PATH', os.pathsep, str(bin_dir))
    _PrependOrInsert(cargo_env, 'CARGO_ENCODED_RUSTFLAGS', chr(0x1f),
                     _GetWindowsToolchainFlags())

    # https://docs.python.org/3/library/subprocess.html#subprocess.Popen:
    #     **Warning**: For maximum reliability, use a fully qualified path for
    #     the executable.
    #
    #     Resolving the path of executable (or the first item of args) is
    #     platform dependent. [...] For Windows, see the documentation of the
    #     `lpApplicationName` and `lpCommandLine` parameters of WinAPI
    #     `CreateProcess`, and note that when resolving or searching for the
    #     executable path with `shell=False`, _cwd_ does not override the
    #     current working directory and _env_ cannot override the `PATH`
    #     environment variable.
    #
    # The `CreateProcessW` documentation states that `lpApplicationName` and
    # `lpCommandLine` uses the **current process'** `PATH` environment variable
    # to look up executables. However, the created process' environment
    # variables can still be specified using `lpEnvironment`.
    #
    # Therefore, there is no need for `shell=True` here if we provide a fully
    # qualified path to cargo.
    return subprocess.run(['cargo'] + cargo_args,
                          env=cargo_env,
                          executable=cargo_path).returncode


def main():
    parser = argparse.ArgumentParser(description='run cargo')
    parser.add_argument('--rust-sysroot',
                        default=DEFAULT_SYSROOT,
                        type=pathlib.Path,
                        help='use cargo and rustc from here')
    (args, cargo_args) = parser.parse_known_args()
    return RunCargo(args.rust_sysroot, None, cargo_args)


if __name__ == '__main__':
    sys.exit(main())

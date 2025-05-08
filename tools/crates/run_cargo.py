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
_CARGO_EXE = 'cargo.exe' if 'STARTUPINFO' in subprocess.__all__ else 'cargo'


def RunCargo(rust_sysroot, home_dir, cargo_args):
    rust_sysroot = pathlib.Path(rust_sysroot)
    if not rust_sysroot.exists():
        print(f'WARNING: Rust sysroot missing at "{rust_sysroot}"')

    bin_dir = rust_sysroot.absolute().joinpath('bin')
    cargo_path = bin_dir.joinpath(_CARGO_EXE)

    cargo_env = dict(os.environ)
    if home_dir:
        cargo_env['CARGO_HOME'] = home_dir
    cargo_env['PATH'] = (f'{bin_dir}{os.pathsep}{cargo_env["PATH"]}'
                         if cargo_env["PATH"] else f'{bin_dir}')

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

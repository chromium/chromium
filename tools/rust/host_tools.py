# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helpers for scripts that build more host tools after `build_rust.py`.

The tools (e.g. `bindgen`) are shipped in the same package as `rustc`, so they
must run on the same minimum supported host.  On Linux, they link against the
Debian sysroot, not against the glibc of the build machine.  Tools that link
the LLVM libraries from `RUST_HOST_LLVM_INSTALL_DIR` must also be built with
the same compiler and against the same sysroot as these libraries.

The tools are built with the `cargo` and `rustc` that `build_rust.py` built
(see `RunHostCargo`).  Note that this is different from
`//tools/crates/run_cargo.py`, which uses the Rust toolchain of a Chromium
checkout, and which doesn't work on the toolchain bots.
"""

import collections
import functools
import os
import sys

from build_rust import (
    CARGO_HOME_DIR,
    GetMacSdkPath,
    RUST_HOST_LLVM_INSTALL_DIR,
    RustTargetTriple,
)
from update_rust import RUST_TOOLCHAIN_OUT_DIR

# Get variables and helpers from Clang update script
sys.path.append(
    os.path.join(
        os.path.dirname(os.path.abspath(__file__)), '..', 'clang', 'scripts'
    )
)

from build import DownloadDebianSysroot, RunCommand

EXE = '.exe' if sys.platform == 'win32' else ''

CARGO_BIN = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'bin', f'cargo{EXE}')
RUSTC_BIN = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'bin', f'rustc{EXE}')


@functools.cache
def GetHostDebianSysroot():
    """Returns the Debian sysroot to build host tools against on Linux.

    The sysroot avoids linking with the system libstdc++ and glibc.
    `build_rust.py` downloads it, so this function only computes the path.
    """
    assert sys.platform.startswith('linux')
    sysroot = DownloadDebianSysroot('amd64', skip_download=True)
    if not os.path.isdir(sysroot):
        raise RuntimeError(
            f'{sysroot} not found.  Run `tools/rust/build_rust.py` first.'
        )
    return sysroot


def GetHostCcTools():
    """Returns the (C compiler, C++ compiler, archiver) for host tools.

    On Windows the MSVC-style `clang-cl` driver is both the C and the C++
    compiler.
    """
    bin_dir = os.path.join(RUST_HOST_LLVM_INSTALL_DIR, 'bin')
    if sys.platform == 'win32':
        clang_cl = os.path.join(bin_dir, 'clang-cl.exe')
        return clang_cl, clang_cl, os.path.join(bin_dir, 'llvm-lib.exe')
    return (
        os.path.join(bin_dir, 'clang'),
        os.path.join(bin_dir, 'clang++'),
        os.path.join(bin_dir, 'llvm-ar'),
    )


def GetHostCFlags():
    """Returns the flags for all C and C++ compilations of host tools."""
    if sys.platform.startswith('linux'):
        return [f'--sysroot={GetHostDebianSysroot()}']
    if sys.platform == 'darwin':
        return ['-isysroot', GetMacSdkPath()]
    return []


def GetHostLinker():
    """Returns the linker for host tools.

    Windows uses `lld-link` for MSVC compat.  Otherwise, we use `lld` via
    `clang` (see `GetHostLinkArgs`).
    """
    bin_dir = os.path.join(RUST_HOST_LLVM_INSTALL_DIR, 'bin')
    if sys.platform == 'win32':
        return os.path.join(bin_dir, 'lld-link.exe')
    return os.path.join(bin_dir, 'clang')


def GetHostLinkArgs():
    """Returns the args for the `clang` linker driver from `GetHostLinker()`."""
    if sys.platform == 'win32':
        # On Windows `GetHostLinker()` is `lld-link` itself (no driver), and it
        # needs no extra args.
        return []
    args = ['-fuse-ld=lld']
    if sys.platform.startswith('linux'):
        args.append(f'--sysroot={GetHostDebianSysroot()}')
    if sys.platform == 'darwin':
        args += ['-isysroot', GetMacSdkPath()]
        if 'x86_64' in RustTargetTriple():
            # `rustc` passes `-nodefaultlibs` to the linker driver, which
            # omits Clang's builtins library.  On x86_64, `libclangLex` needs
            # `___cpu_model` from it.  With `-nodefaultlibs`, this flag links
            # only the builtins library.
            args.append('-fapple-link-rtlib')
    return args


def GetHostLinkerRustFlags():
    """Returns the rustflags to link host tools with `GetHostLinker()`."""
    return [f'-Clinker={GetHostLinker()}'] + [
        f'-Clink-arg={arg}' for arg in GetHostLinkArgs()
    ]


def GetHostCargoEnv(cargo_home=CARGO_HOME_DIR):
    """Returns the environment for `RunHostCargo`.

    The result is a `collections.defaultdict(str)`, so callers can extend it
    before passing it to `RunHostCargo` - e.g. `env['CFLAGS'] += ' -Ifoo'`.
    Flags in `CFLAGS`, `CXXFLAGS`, `LDFLAGS`, and `RUSTFLAGS` are separated by
    spaces, and are added to the values from the caller's environment.
    """
    env = collections.defaultdict(str, os.environ)
    # Cargo normally stores files in $HOME. Override this.
    env['CARGO_HOME'] = cargo_home

    # Use a rustc we deterministically provide, not a system one.
    env['RUSTC'] = RUSTC_BIN

    # Use the clang compiler from the rustc build.
    cc, cxx, ar = GetHostCcTools()
    env['CC'] = cc
    env['CXX'] = cxx
    env['AR'] = ar
    env['LD'] = GetHostLinker()

    env['CFLAGS'] += ' ' + ' '.join(GetHostCFlags())
    env['CXXFLAGS'] += ' ' + ' '.join(GetHostCFlags())
    env['LDFLAGS'] += ' ' + ' '.join(GetHostLinkArgs())
    env['RUSTFLAGS'] += ' ' + ' '.join(GetHostLinkerRustFlags())
    if sys.platform == 'win32':
        # Link the C runtime statically, like `rustc.exe` and the LLVM
        # libraries (`/MT`), so that the tools need no `VCRUNTIME140.dll`.
        # This also makes the `cc` crate use `/MT`.
        env['RUSTFLAGS'] += ' -Ctarget-feature=+crt-static'
    return env


def RunHostCargo(cargo_args, env):
    """Runs the `cargo` that `build_rust.py` built, with `cargo_args`.

    `env` is the complete environment for `cargo`.  Usually it is the
    result of `GetHostCargoEnv`, with more settings from the caller.

    On Windows, `setenv=True` gives `cargo` and everything that it runs (e.g.
    the `cc` crate and `lld-link`) the hermetic MSVC environment (e.g.
    `INCLUDE` and `LIB`).

    This will `fail_hard` and not return if `cargo` reports problems.
    """
    for tool in [CARGO_BIN, RUSTC_BIN]:
        if not os.path.exists(tool):
            print(
                f'Missing {tool}. This script expects to be run after '
                f'build_rust.py is run as the build_rust.py script builds '
                f'cargo and rustc that are needed here.'
            )
            sys.exit(1)

    RunCommand([CARGO_BIN] + cargo_args, setenv=True, env=env)

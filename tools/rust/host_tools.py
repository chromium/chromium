# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helpers for scripts that build more host tools after `build_rust.py`.

The tools (e.g. `bindgen`) are shipped in the same package as `rustc`, so they
must run on the same minimum supported host.  On Linux, they link against the
Debian sysroot, not against the glibc of the build machine.  Tools that link
the LLVM libraries from `RUST_HOST_LLVM_INSTALL_DIR` must also be built with
the same compiler and against the same sysroot as these libraries.
"""

import functools
import os
import sys

from build_rust import (
    GetMacSdkPath,
    RUST_HOST_LLVM_INSTALL_DIR,
    RustTargetTriple,
)

# Get variables and helpers from Clang update script
sys.path.append(
    os.path.join(
        os.path.dirname(os.path.abspath(__file__)), '..', 'clang', 'scripts'
    )
)

from build import DownloadDebianSysroot


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

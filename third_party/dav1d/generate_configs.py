#!/usr/bin/env python3
#
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates config files for building dav1d."""

from __future__ import print_function

import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

BASE_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_ROOT_DIR = os.path.abspath(os.path.join(BASE_DIR, '..', '..'))

sys.path.append(os.path.join(CHROMIUM_ROOT_DIR, 'build'))
import gn_helpers

MESON = ['meson']

DEFAULT_BUILD_ARGS = [
    '-Denable_tools=false', '-Denable_tests=false', '-Ddefault_library=static',
    '--buildtype', 'release'
]

WINDOWS_BUILD_ARGS = ['-Dc_winlibs=']


def PrintAndCheckCall(argv, *args, **kwargs):
    print('\n-------------------------------------------------\nRunning %s' %
          ' '.join(argv))
    c = subprocess.check_call(argv, *args, **kwargs)


def RewriteFile(path, search_replace):
    with open(path) as f:
        contents = f.read()
    with open(path, 'w') as f:
        for search, replace in search_replace:
            contents = re.sub(search, replace, contents)

        # Cleanup trailing newlines.
        f.write(contents.strip() + '\n')


def SetupWindowsCrossCompileToolchain(target_arch):
    # First retrieve various MSVC and Windows SDK paths.
    output = subprocess.check_output([
        'python3',
        os.path.join(CHROMIUM_ROOT_DIR, 'build', 'vs_toolchain.py'),
        'get_toolchain_dir'
    ],
                                     universal_newlines=True)

    # Turn this into a dictionary.
    win_dirs = gn_helpers.FromGNArgs(output)

    # Use those paths with a second script which will tell us the proper include
    # and lib paths to specify for cflags and ldflags respectively.
    output = subprocess.check_output([
        'python3',
        os.path.join(CHROMIUM_ROOT_DIR, 'build', 'toolchain', 'win',
                     'setup_toolchain.py'), win_dirs['vs_path'],
        win_dirs['sdk_path'], win_dirs['runtime_dirs'], 'win', target_arch,
        'none'
    ],
                                     universal_newlines=True)

    flags = gn_helpers.FromGNArgs(output)
    cwd = os.getcwd()

    target_env = os.environ

    # Each path is of the form:
    # "/I../depot_tools/win_toolchain/vs_files/20d5f2553f/Windows Kits/10/Include/10.0.19041.0/winrt"
    #
    # Since there's a space in the include path, inputs are quoted in |flags|, we
    # can helpfully use shlex to split on spaces while preserving quoted strings.
    include_paths = []
    for include_path in shlex.split(flags['include_flags_I']):
        # Apparently setup_toolchain prefers relative include paths, which
        # may work for chrome, but it does not work for dav1d, so let's make
        # them asbolute again.
        include_path = os.path.abspath(os.path.join(cwd, include_path[2:]))
        include_paths.append(include_path)

    SYSROOT_PREFIX = '/winsysroot:'
    for k in flags:
        if SYSROOT_PREFIX in flags[k]:
            target_env['WINSYSROOT'] = os.path.abspath(
                os.path.join(cwd, flags[k][len(SYSROOT_PREFIX):]))
            break

    target_env = os.environ
    target_env['INCLUDE'] = ';'.join(include_paths)
    return target_env


def CopyConfigs(src_dir, dest_dir):
    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)

    shutil.copy(os.path.join(src_dir, 'config.h'), dest_dir)

    # The .asm file will not be present for all configurations.
    asm_file = os.path.join(src_dir, 'config.asm')
    if os.path.exists(asm_file):
        shutil.copy(asm_file, dest_dir)


def GenerateConfig(config_dir, env, special_args=[]):
    temp_dir = tempfile.mkdtemp()
    PrintAndCheckCall(MESON + ['setup'] + DEFAULT_BUILD_ARGS + special_args +
                      [temp_dir],
                      cwd='libdav1d',
                      env=env)

    RewriteFile(
        os.path.join(temp_dir, 'config.h'),
        [
            # We don't want non-visible log strings polluting the official binary.
            (r'(#define CONFIG_LOG .*)',
             r'// \1 -- Logging is controlled by Chromium'),

            # The Chromium build system already defines this.
            (r'(#define _WIN32_WINNT .*)',
             r'// \1 -- Windows version is controlled by Chromium'),

            # Android doesn't have pthread_{get,set}affinity_np.
            (r'(#define HAVE_PTHREAD_(GET|SET)AFFINITY_NP \d{1,2})',
             r'// \1 -- Controlled by Chomium'),
        ])

    config_asm_path = os.path.join(temp_dir, 'config.asm')
    if (os.path.exists(config_asm_path)):
        RewriteFile(
            config_asm_path,
            # Clang LTO doesn't respect stack alignment, so we must use
            # the platform's default stack alignment;
            # https://crbug.com/928743.
            [(r'(%define STACK_ALIGNMENT \d{1,2})',
              r'; \1 -- Stack alignment is controlled by Chromium')])

    CopyConfigs(temp_dir, config_dir)

    shutil.rmtree(temp_dir)


def GenerateAppleConfig(src_dir, dest_dir):
    CopyConfigs(src_dir, dest_dir)

    RewriteFile(
        os.path.join(dest_dir, 'config.h'),
        [
            # Apple doesn't have the <sys/auxv.h> header.
            (r'#define HAVE_GETAUXVAL 1', r'#define HAVE_GETAUXVAL 0'),
            # Apple doesn't have the memalign() function.
            (r'#define HAVE_MEMALIGN 1', r'#define HAVE_MEMALIGN 0'),
        ])


def GenerateWindowsArm64Config(src_dir):
    win_arm64_dir = 'config/win/arm64'
    if not os.path.exists(win_arm64_dir):
        os.makedirs(win_arm64_dir)

    shutil.copy(os.path.join(src_dir, 'config.h'), win_arm64_dir)

    # Flip flags such that it looks like an arm64 configuration.
    RewriteFile(os.path.join(win_arm64_dir, 'config.h'),
                [(r'#define ARCH_X86 1', r'#define ARCH_X86 0'),
                 (r'#define ARCH_X86_64 1', r'#define ARCH_X86_64 0'),
                 (r'#define ARCH_AARCH64 0', r'#define ARCH_AARCH64 1')])


def GenerateGenericConfig(src_dir):
    generic_dir = 'config/linux-noasm/generic'
    if not os.path.exists(generic_dir):
        os.makedirs(generic_dir)

    shutil.copy(os.path.join(src_dir, 'config.h'), generic_dir)

    # Mark architecture as unknown.
    RewriteFile(os.path.join(generic_dir, 'config.h'),
                [(r'#define ARCH_X86 1', r'#define ARCH_X86 0'),
                 (r'#define ARCH_X86_64 1', r'#define ARCH_X86_64 0')])


def CopyVersions(src_dir, dest_dir):
    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)

    shutil.copy(os.path.join(src_dir, 'include', 'vcs_version.h'), dest_dir)


def GenerateVersion(version_dir, env):
    temp_dir = tempfile.mkdtemp()
    PrintAndCheckCall(MESON + ['setup'] + DEFAULT_BUILD_ARGS + [temp_dir],
                      cwd='libdav1d',
                      env=env)
    PrintAndCheckCall(['ninja', '-C', temp_dir, 'include/vcs_version.h'],
                      cwd='libdav1d',
                      env=env)

    CopyVersions(temp_dir, version_dir)

    shutil.rmtree(temp_dir)


def main():
    linux_env = os.environ
    linux_env['CC'] = 'clang'

    GenerateConfig('config/linux/x64', linux_env)

    noasm_dir = 'config/linux-noasm/x64'
    GenerateConfig(noasm_dir, linux_env, ['-Denable_asm=false'])
    GenerateGenericConfig(noasm_dir)

    GenerateConfig('config/linux/x86', linux_env,
                   ['--cross-file', '../crossfiles/linux32.crossfile'])
    GenerateConfig('config/linux/arm', linux_env,
                   ['--cross-file', '../crossfiles/arm.crossfile'])
    GenerateConfig('config/linux/arm64', linux_env,
                   ['--cross-file', '../crossfiles/arm64.crossfile'])

    win_x86_env = SetupWindowsCrossCompileToolchain('x86')
    GenerateConfig('config/win/x86', win_x86_env,
                   ['--cross-file', '../crossfiles/win32.crossfile'] + [
                       '-Dc_args=-m32 -fuse-ld=lld /winsysroot ' +
                       win_x86_env['WINSYSROOT']
                   ] + WINDOWS_BUILD_ARGS)

    win_x64_dir = 'config/win/x64'
    win_x64_env = SetupWindowsCrossCompileToolchain('x64')
    GenerateConfig(
        win_x64_dir, win_x64_env,
        ['--cross-file', '../crossfiles/win64.crossfile'] +
        ['-Dc_args=-fuse-ld=lld /winsysroot ' + win_x64_env['WINSYSROOT']] +
        WINDOWS_BUILD_ARGS)

    # Sadly meson doesn't support arm64 + clang-cl, so we need to create the
    # Windows arm64 config from the Windows x64 config.
    GenerateWindowsArm64Config(win_x64_dir)

    # Create the Apple configs from the Linux configs.
    GenerateAppleConfig('config/linux/arm', 'config/apple/arm')
    GenerateAppleConfig('config/linux/arm64', 'config/apple/arm64')
    GenerateAppleConfig('config/linux/x64', 'config/apple/x64')
    GenerateAppleConfig('config/linux/x86', 'config/apple/x86')

    GenerateVersion('version', linux_env)


if __name__ == '__main__':
    main()

#!/usr/bin/env python
#
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates config files for building Weston."""

from __future__ import print_function

import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile


BASE_DIR = os.path.abspath(os.path.dirname(__file__))

CHROMIUM_ROOT_DIR = os.path.abspath(os.path.join(BASE_DIR, '..', '..'))

CLANG_DIR = os.path.join(CHROMIUM_ROOT_DIR, 'third_party', 'llvm-build', 'Release+Asserts', 'bin')

sys.path.append(os.path.join(CHROMIUM_ROOT_DIR, 'build'))

import gn_helpers

MESON = ['meson', 'setup']

DEFAULT_BUILD_ARGS = [
    '--buildtype', 'release',
    '-Dbackend-drm=false',
    '-Dbackend-drm-screencast-vaapi=false',
    '-Dbackend-pipewire=false',
    '-Dbackend-rdp=false',
    '-Dscreenshare=false',
    '-Dbackend-vnc=false',
    '-Dbackend-default=auto',
    '-Dxwayland=false',
    '-Dremoting=false',
    '-Dpipewire=false',
    '-Dshell-ivi=false',
    '-Dcolor-management-lcms=false',
    '-Dimage-jpeg=false',
    '-Dimage-webp=false',
    '-Ddemo-clients=false',
    '-Dsimple-clients=egl',
    '-Dwcap-decode=false',
]


def GetAbsPath(relative_path):
    return os.path.join(BASE_DIR, relative_path)


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

def AddAttributeInConfig(path):
    with open(path) as f:
        contents = f.read()
    with open(path, 'w') as f:
        f.write(contents.strip() + '\n')
        f.write('\n' + '__attribute__((visibility("default"))) int main(int argc, char* argv[]);' + '\n')

def CopyConfigsAndCleanup(config_dir, dest_dir):
    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)

    shutil.copy(os.path.join(config_dir, 'config.h'), dest_dir)
    shutil.rmtree(config_dir)


def RewriteGitFile(path, data):
    with open(path, 'w') as f:
        contents = data

        # Cleanup trailing newlines.
        f.write(contents.strip() + '\n')


def CopyGitConfigsAndCleanup(config_dir, dest_dir):
    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)

    shutil.copy(os.path.join(config_dir, 'git-version.h'), dest_dir)
    shutil.rmtree(config_dir)


def GenerateGitConfig(config_dir, env, special_args=[]):
    temp_dir = tempfile.mkdtemp()
    PrintAndCheckCall(
        MESON + DEFAULT_BUILD_ARGS + special_args + [temp_dir],
        cwd=GetAbsPath('src'),
        env=env)

    label = subprocess.check_output(["git", "describe", "--always"]).strip()
    label = label.decode("utf-8")
    RewriteGitFile(
        os.path.join(temp_dir, 'git-version.h'),
        "#define BUILD_ID \"{label}\"".format(label=label))
    CopyGitConfigsAndCleanup(temp_dir, config_dir)


def GenerateConfig(config_dir, env, special_args=[]):
    temp_dir = tempfile.mkdtemp()
    PrintAndCheckCall(
        MESON + DEFAULT_BUILD_ARGS + special_args + [temp_dir],
        cwd=GetAbsPath('src'),
        env=env)

    CopyConfigsAndCleanup(temp_dir, config_dir)


def ChangeConfigPath():
    configfile = GetAbsPath("config/config.h")
    DIRS = ["BINDIR",
            "DATADIR",
            "LIBEXECDIR",
            "LIBWESTON_MODULEDIR",
            "MODULEDIR"]
    for dir in DIRS:
        pattern = "#define {dir} \"/[a-zA-Z0-9\\-_/]+\"".format(dir=dir)
        RewriteFile(configfile, [(pattern, "")])

    # Add attribute in config.h to suppress all undefined symbol(function) warnings
    AddAttributeInConfig(configfile)


def GenerateWestonVersion():
    dirname = GetAbsPath("version/libweston")
    if not os.path.exists(dirname):
        os.makedirs(dirname)
    version_op_file = GetAbsPath("version/libweston/version.h")
    configfile = GetAbsPath("config/config.h")
    version_in_file = GetAbsPath("src/include/libweston/version.h.in")
    version_number = "0.0.0"
    with open(configfile, 'r') as f:
        for line in f:
            if "PACKAGE_VERSION" in line:
                package_version_list = (line.strip("\n")).split(" ")
                version_number = package_version_list[-1]

    version_number_list = (version_number.strip('"\n"')).split(".")
    version_number_list.append(version_number.strip("\"\""))
    VERSIONS = ["@WESTON_VERSION_MAJOR@", "@WESTON_VERSION_MINOR@",
                "@WESTON_VERSION_MICRO@", "@WESTON_VERSION@"]
    with open(version_in_file) as f:
        contents = f.read()

    for version, version_number in zip(VERSIONS, version_number_list):
        pattern = version
        repl_string = version_number
        with open(version_op_file, 'w') as f:
            contents = re.sub(pattern, repl_string, contents)

            # Cleanup trailing newlines.
            f.write(contents.strip() + '\n')
    print("Created version.h file from version.h.in\n")


def RemoveUndesiredDefines():
    configfile = GetAbsPath('config/config.h')
    # Weston doesn't have a meson option to avoid using memfd_create() method that was
    # introduced in GLIBC 2.27. That results in weston failing to run on Xenial based bot as
    # it has GLIBC 2.23, because this config might be generated on a system that has newer
    # libc libraries that meson checks with has_function() method. Thus, explicitly rewrite
    # the config to disable usage of that method.
    RewriteFile(configfile, [("#define HAVE_MEMFD_CREATE .*", "")])


def main():
    env = os.environ
    env['CC'] = 'clang'
    env['PATH'] = ':'.join([CLANG_DIR, env['PATH']])
    GenerateGitConfig(GetAbsPath('version'), env)
    GenerateConfig(GetAbsPath('config'), env)
    ChangeConfigPath()
    RemoveUndesiredDefines()
    GenerateWestonVersion()

if __name__ == '__main__':
    main()

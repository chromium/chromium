#!/usr/bin/env python
#
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates config files for building libwayland."""

import os
import shutil
import subprocess
import tempfile


MESON = ['meson']

DEFAULT_BUILD_ARGS = [
    '-Ddocumentation=false',
    '-Dtests=false',
]


def GetAbsPath(relative_path):
    return os.path.join(os.path.abspath(os.path.dirname(__file__)), relative_path)


def PrintAndCheckCall(argv, *args, **kwargs):
    print('\n-------------------------------------------------\nRunning %s' %
          ' '.join(argv))
    c = subprocess.check_call(argv, *args, **kwargs)


def CallMesonGenerator(build_dir):
    PrintAndCheckCall(
        MESON + DEFAULT_BUILD_ARGS + [build_dir],
        cwd=GetAbsPath('src'),
        env=os.environ)


def CopyFileToDestination(file, src_dir, dest_dir):
    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)

    shutil.copy(os.path.join(src_dir, file), dest_dir)
    print("Copied %s to %s from %s" % (file, dest_dir, src_dir))


def main():
    # Creates a directory that will be used by meson to generate build configs.
    temp_dir = tempfile.mkdtemp()

    # Calls meson for //third_party/wayland/src and generates build files.
    CallMesonGenerator(temp_dir)
    # Copies config.h to //third_party/wayland/include
    CopyFileToDestination('config.h', temp_dir, GetAbsPath('include'))
    # Copies wayland-version.h to //third_party/wayland/include/src
    CopyFileToDestination('wayland-version.h', temp_dir + '/src', GetAbsPath('include/src'))

    # Removes the directory we used for meson config.
    shutil.rmtree(temp_dir)


if __name__ == '__main__':
    main()

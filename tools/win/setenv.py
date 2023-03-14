# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Helper script to do the heavy lifting for setenv.bat.
"""

from __future__ import print_function

import os
import sys

script_path = os.path.abspath(os.path.dirname(__file__))
build_path = os.path.normpath(os.path.join(script_path, '..', '..', 'build'))
sys.path.append(build_path)

import vs_toolchain

if bool(int(os.environ.get('DEPOT_TOOLS_WIN_TOOLCHAIN', '1'))):
    win_sdk_dir = vs_toolchain.SetEnvironmentAndGetSDKDir()
    script_path = os.path.normpath(os.path.join(win_sdk_dir, 'bin/SetEnv.cmd'))
    print('"%s" /x64' % script_path)
else:
    vs_version = vs_toolchain.GetVisualStudioVersion()
    vs_path = vs_toolchain.DetectVisualStudioPath()
    if vs_version in ['2022']:
        script_path = os.path.join(vs_path,
                                   r'VC\Auxiliary\Build\vcvarsall.bat')
        print('"%s" amd64' % script_path)
    else:
        raise Exception('Unknown VS version %s' % vs_version)

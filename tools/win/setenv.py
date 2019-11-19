# Copyright (c) 2017 The Chromium Authors. All rights reserved.
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
  print(os.path.normpath(os.path.join(win_sdk_dir, 'bin/SetEnv.cmd')))
else:
  vs_version = vs_toolchain.GetVisualStudioVersion()
  vs_path = vs_toolchain.DetectVisualStudioPath()
  if vs_version == '2017':
    print(os.path.join(vs_path, r'VC\Auxiliary\Build\vcvarsall.bat'))
  elif vs_version == '2015':
    print(os.path.join(vs_path, r'VC\vcvarsall.bat'))
  else:
    raise Exception('Unknown VS version %s' % vs_version)

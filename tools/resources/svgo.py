#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function


def Run(os_path=None, args=None):
  _HERE_PATH = os_path.dirname(os_path.realpath(__file__))
  _SRC_PATH = os_path.normpath(os_path.join(_HERE_PATH, '..', '..'))

  import sys
  old_sys_path = sys.path[:]
  sys.path.append(os_path.join(_SRC_PATH, 'third_party', 'node'))

  try:
    import node, node_modules
  finally:
    sys.path = old_sys_path

  # Removing viewBox is not always safe, since it assumes that width/height are
  # not overriden in all usages of an SVG file. Feel free to remove viewBox
  # manually from a certain SVG if you have audited all its usages.
  default_args = ['--disable=removeViewBox'];
  return node.RunNode([node_modules.PathToSvgo()] + default_args + args)


if __name__ == '__main__':
  import os
  import sys
  print(Run(os_path=os.path, args=sys.argv[1:]))

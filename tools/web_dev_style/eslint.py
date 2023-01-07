#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def Run(os_path=None, args=None):
  try:
    _HERE_PATH = os_path.dirname(os_path.realpath(__file__))
    _SRC_PATH = os_path.normpath(os_path.join(_HERE_PATH, '..', '..'))
    _NODE_PATH = os_path.join(_SRC_PATH, 'third_party', 'node')

    import sys
    old_sys_path = sys.path[:]
    sys.path.append(_NODE_PATH)

    import node, node_modules
  finally:
    sys.path = old_sys_path

  return node.RunNode([
      node_modules.PathToEsLint(),
      '--quiet',
      '--resolve-plugins-relative-to',
      os_path.join(_NODE_PATH, 'node_modules'),
  ] + args)


if __name__ == '__main__':
  import os
  import sys
  Run(os_path=os.path, args=sys.argv[1:])

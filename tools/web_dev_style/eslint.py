#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

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

  # When '--config' is passed, ESLint uses cwd as the base path for all
  # 'ignorePatterns' (v8 config) or 'ignores' (v9 config), and cannot correctly
  # navigate parent directories via '../'. We must set the repository's root as
  # the cwd.
  os.chdir(_SRC_PATH)
  return node.RunNode([
      node_modules.PathToEsLint(),
      '--quiet',
      '--config',
      os_path.join(_HERE_PATH, 'eslint.config.mjs'),
  ] + args)


if __name__ == '__main__':
  import os
  import sys
  Run(os_path=os.path, args=sys.argv[1:])

#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def Run(os_path=None, args=None):
  try:
    _HERE_PATH = os_path.dirname(os_path.realpath(__file__))
    _SRC_PATH = os_path.normpath(os_path.join(_HERE_PATH, '..', '..'))
    _NODE_PATH = os_path.join(_SRC_PATH, 'third_party', 'node')

    import os
    import sys
    old_sys_path = sys.path[:]
    sys.path.append(_NODE_PATH)

    import node, node_modules
  finally:
    sys.path = old_sys_path

  # When running git cl presubmit --all this presubmit may be asked to check
  # ~1,100 files, leading to a command line that is about 92,000 characters.
  # This goes past the Windows 8191 character cmd.exe limit and causes cryptic
  # failures. To avoid these we break the command up into smaller pieces. The
  # non-Windows limit is chosen so that the code that splits up commands will
  # get some exercise on other platforms.
  # Depending on how long the command is on Windows the error may be:
  #     The command line is too long.
  # Or it may be:
  #     OSError: Execution failed with error: [WinError 206] The filename or
  #     extension is too long.
  # I suspect that the latter error comes from CreateProcess hitting its 32768
  # character limit.
  files_per_command = 50 if os.name == 'nt' else 1000
  results = []
  for i in range(0, len(args), files_per_command):
    results.append(
        node.RunNode([
            node_modules.PathToEsLint(),
            '--resolve-plugins-relative-to',
            os_path.join(_NODE_PATH, 'node_modules'),
        ] + args[i:i + files_per_command]))
  return results


if __name__ == '__main__':
  import os
  import sys
  Run(os_path=os.path, args=sys.argv[1:])

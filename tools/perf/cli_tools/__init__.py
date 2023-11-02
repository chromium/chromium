# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Command line tools.

This module contains implementations for several performance related command
line tools.

Each submodule is a largely independent tool, which should expose a Main()
function to be called by a thin executable script made available in the parent
directory.

For example, the code of `tools/perf/my_fancy_tool` should mostly be:

    #!/usr/bin/env vpython3
    import sys
    from command_line_tools import my_fancy_tool

    if __name__ == '__main__':
      sys.exit(my_fancy_tool.Main())

Reusable code that could be shared by many of these tools should be placed
under `tools/perf/core`.
"""

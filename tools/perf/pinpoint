#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import subprocess
import sys

from core import path_util

cb_dir = pathlib.Path(path_util.GetCrossBenchDir())
cb_path = cb_dir / "cb.py"

if __name__ == '__main__':
  argv = sys.argv
  if '--version' in argv[1:]:
    cmd = (cb_path, '--version')
  else:
    cmd = (cb_path, "pinpoint", *argv[1:])
  result = subprocess.run(cmd)
  sys.exit(result.returncode)

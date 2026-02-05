#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import subprocess
import sys


CHROMIUM_SRC_DIR = pathlib.Path(__file__).absolute().parents[2]
THIRD_PARTY_DIR = CHROMIUM_SRC_DIR / 'third_party'
CROSSBENCH_DIR = THIRD_PARTY_DIR / 'crossbench'
CUJ_RUNNER = (THIRD_PARTY_DIR /
              'crossbench-web-tests/cuj/crossbench/runner/run.py')


def main():
  # Set PYTHONPATH so that the CUJ runner can find crossbench.
  env = os.environ.copy()
  env['PYTHONPATH'] = CROSSBENCH_DIR

  # TODO(b:435031130): For initial testing, only run a single benchmark.
  command_line = [CUJ_RUNNER, '--platform=cros', '--tests=speeometer3.1']
  proc = subprocess.run(command_line, check=False, env=env)
  status = proc.returncode

  # TODO(b:435031130): While this feature is still in development, we just log
  # the status code and then always return success. We will return the real
  # status code only after the code has been verified to be reliable.
  print('Web-tests CUJ runner returned status', status)
  return 0


if __name__ == '__main__':
  sys.exit(main())

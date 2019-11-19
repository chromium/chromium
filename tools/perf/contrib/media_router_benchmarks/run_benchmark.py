# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

# Add //tools/perf/ to system path.
sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
from chrome_telemetry_build import chromium_config

from core import benchmark_runner
from core import path_util


def main():
  config = chromium_config.ChromiumConfig(
      benchmark_dirs=[path_util.GetOfficialBenchmarksDir(),
                      path_util.GetContribDir()],
      top_level_dir=path_util.GetPerfDir(),
      expectations_files=[path_util.GetExpectationsPath()])
  return benchmark_runner.main(config)


if __name__ == '__main__':
  sys.exit(main())

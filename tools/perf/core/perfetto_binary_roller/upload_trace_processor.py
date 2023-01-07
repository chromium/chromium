#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import sys
import time

# Add tools/perf to sys.path.
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))

from core import path_util
path_util.AddPyUtilsToPath()
path_util.AddTracingToPath()

from core.perfetto_binary_roller import binary_deps_manager
from core.tbmv3 import trace_processor


def _PerfettoRevision():
  deps_line_re = re.compile(
      r".*'/platform/external/perfetto.git' \+ '@' \+ '([a-f0-9]+)'")
  deps_file = os.path.join(path_util.GetChromiumSrcDir(), 'DEPS')
  with open(deps_file) as deps:
    for line in deps:
      match = deps_line_re.match(line)
      if match:
        return match.group(1)
  raise RuntimeError("Couldn't parse perfetto revision from DEPS")


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--path', help='Path to trace_processor_shell binary.', required=True)
  parser.add_argument(
      '--revision',
      help=('Perfetto revision. '
            'If not supplied, will try to infer from DEPS file.'))
  parser.add_argument('--isolated-script-test-output',
                      help='Path to the output file.')

  args = parser.parse_args(args)

  revision = args.revision or _PerfettoRevision()

  binary_deps_manager.UploadHostBinaryChromium(trace_processor.TP_BINARY_NAME,
                                               args.path, revision)

  # CI bot expects a valid JSON object as script output.
  if args.isolated_script_test_output is not None:
    with open(args.isolated_script_test_output, 'w') as f:
      f.write('''{
          "interrupted": false,
          "num_failures_by_type": {
              "FAIL": 0,
              "PASS": 1
          },
          "seconds_since_epoch": %s,
          "tests": {
               "upload_trace_processor": {
                   "actual": "PASS",
                   "expected": "PASS"
               }
          },
          "version": 3
      }''' % time.time())


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

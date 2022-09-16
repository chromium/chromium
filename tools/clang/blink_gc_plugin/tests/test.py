#!/usr/bin/env python3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import subprocess
import sys

script_dir = os.path.dirname(os.path.realpath(__file__))
tool_dir = os.path.abspath(os.path.join(script_dir, '../../pylib'))
sys.path.insert(0, tool_dir)

from clang import plugin_testing


class BlinkGcPluginTest(plugin_testing.ClangPluginTest):
  """Test harness for the Blink GC plugin."""

  def __init__(self, use_cppgc, *args, **kwargs):
    super(BlinkGcPluginTest, self).__init__(*args, **kwargs)
    self.use_cppgc = use_cppgc

  def AdjustClangArguments(self, clang_cmd):
    clang_cmd.append('-Wno-inaccessible-base')
    if self.use_cppgc:
      clang_cmd.append('-DUSE_V8_OILPAN')

  def ProcessOneResult(self, test_name, actual):
    # Some Blink GC plugins dump a JSON representation of the object graph, and
    # use the processed results as the actual results of the test.
    if os.path.exists('%s.graph.json' % test_name):
      try:
        actual = subprocess.check_output([
            sys.executable, '../process-graph.py', '-c',
            '%s.graph.json' % test_name
        ],
                                         stderr=subprocess.STDOUT,
                                         universal_newlines=True)
      except subprocess.CalledProcessError as e:
        # The graph processing script returns a failure exit code if the graph
        # is bad (e.g. it has a cycle). The output still needs to be captured in
        # that case, since the expected results capture the errors.
        actual = e.output
      finally:
        # Clean up the .graph.json file to prevent false passes from stale
        # results from a previous run.
        os.remove('%s.graph.json' % test_name)
    if self.use_cppgc:
      if os.path.exists('%s.cppgc.txt' % test_name):
        # Some tests include namespace names in the output and thus require a
        # different output file for comparison.
        test_name = '%s.cppgc' % test_name
    return super(BlinkGcPluginTest, self).ProcessOneResult(test_name, actual)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--reset-results',
      action='store_true',
      help='If specified, overwrites the expected results in place.')
  parser.add_argument('clang_path', help='The path to the clang binary.')
  args = parser.parse_args()

  dir_name = os.path.dirname(os.path.realpath(__file__))

  num_faliures_blink = BlinkGcPluginTest(
      False,  # USE_V8_OILPAN
      dir_name,
      args.clang_path,
      'blink-gc-plugin',
      args.reset_results).Run()

  num_faliures_cppgc = BlinkGcPluginTest(
      True,  # USE_V8_OILPAN
      dir_name,
      args.clang_path,
      'blink-gc-plugin',
      args.reset_results).Run()

  print("\nBlink GC Plugin Summary: %d tests failed without USE_V8_OILPAN, " \
   "%d tests failed with USE_V8_OILPAN" % (
      num_faliures_blink, num_faliures_cppgc))
  return num_faliures_blink + num_faliures_cppgc


if __name__ == '__main__':
  sys.exit(main())

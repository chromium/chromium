#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
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


class IteratorCheckerPluginTest(plugin_testing.ClangPluginTest):
  """Test harness for the Iterator Checker plugin."""

  def __init__(self, *args, **kwargs):
    super(IteratorCheckerPluginTest, self).__init__(*args, **kwargs)

  def AdjustClangArguments(self, clang_cmd):
    pass

  def ProcessOneResult(self, test_name, actual):
    return super(IteratorCheckerPluginTest,
                 self).ProcessOneResult(test_name, actual)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--reset-results',
      action='store_true',
      help='If specified, overwrites the expected results in place.')
  parser.add_argument('clang_path', help='The path to the clang binary.')
  args = parser.parse_args()

  dir_name = os.path.dirname(os.path.realpath(__file__))

  num_failures = IteratorCheckerPluginTest(dir_name, args.clang_path,
                                           ['iterator-checker'],
                                           args.reset_results).Run()

  return num_failures


if __name__ == '__main__':
  sys.exit(main())

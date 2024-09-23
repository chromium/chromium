#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
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


class ChromeStylePluginTest(plugin_testing.ClangPluginTest):
  """Test harness for the Chrome style plugin."""

  def AdjustClangArguments(self, clang_cmd):
    clang_cmd.extend([
        # Skip code generation
        '-fsyntax-only',
        # Fake system directory for tests
        '-isystem',
        os.path.join(os.getcwd(), 'system'),
        '-Wno-inconsistent-missing-override',
        '--include-directory',
        '.',
    ])


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--reset-results',
      action='store_true',
      help='If specified, overwrites the expected results in place.')
  parser.add_argument('clang_path', help='The path to the clang binary.')
  parser.add_argument('--filter',
                      action='store',
                      help='Filter to test files that match a regex')
  args = parser.parse_args()

  return ChromeStylePluginTest(os.path.dirname(os.path.realpath(__file__)),
                               args.clang_path,
                               ['find-bad-constructs', 'unsafe-buffers'],
                               args.reset_results,
                               filename_regex=args.filter).Run()


if __name__ == '__main__':
  sys.exit(main())

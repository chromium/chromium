# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script provides the necessary flags to symbolize proto traces.
"""

import optparse


def AddSymbolizeOptions(parser):
  """Add options for symbolizing proto traces.

  Args:
    parser: OptionParser object for parsing the command line.

  Returns:
    Input |parser| with added symbolization options.
  """
  parser.add_option(
      '--trace_processor_path',
      help=('Optional path to trace processor binary. '
            'Automatically downloads binary if flag not specified.'),
      dest='trace_processor_path')
  parser.add_option(
      '--breakpad_output_dir',
      help=('Optional path to empty directory to store fetched trace symbols. '
            'Automatically uses temporary directory if flag not specified.'),
      dest='breakpad_output_dir')
  parser.add_option(
      '--cloud_storage_bucket',
      help=('Optional bucket in cloud storage where symbols reside. '
            "Defaults to 'chrome-unsigned'."),
      dest='cloud_storage_bucket',
      default='chrome-unsigned')

  return parser

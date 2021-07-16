# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script provides the necessary flags to symbolize proto traces.
"""

import optparse
import logging


def SymbolizeOptions(parser):
  """Build option group for proto trace symbolization.

  Args:
    parser: OptionParser object for parsing the command line.

  Returns:
    Option group that contains symbolization options.
  """
  symbolization_options = optparse.OptionGroup(parser, 'Symbolization Options')
  symbolization_options.add_option(
      '--trace_processor_path',
      help=('Optional path to trace processor binary. '
            'Automatically downloads binary if flag not specified.'),
      dest='trace_processor_path')
  symbolization_options.add_option(
      '--breakpad_output_dir',
      help=('Optional path to empty directory to store fetched trace symbols. '
            'Automatically uses temporary directory if flag not specified.'),
      dest='breakpad_output_dir')
  symbolization_options.add_option(
      '--cloud_storage_bucket',
      help=('Optional bucket in cloud storage where symbols reside. '
            "Defaults to 'chrome-unsigned'."),
      dest='cloud_storage_bucket',
      default='chrome-unsigned')

  return symbolization_options


def AddLoggingOptions(parser):
  """Add options for logging to |parser|.

  Args:
    parser: OptionParser object for parsing the command line.

  Returns:
    Input |parser| with added logging options.
  """
  parser.add_option('-v',
                    '--verbose',
                    help='Increase output verbosity.',
                    action='count',
                    dest='verbosity')
  return parser


def SetupLogging(verbosity):
  """Setup logging levels.

  Sets default level to warning.

  Args:
    verbosity: None or integer type that specifies the
      logging level. (options.verbosity)
  """
  if verbosity == 1:
    logging.basicConfig(level=logging.INFO)
  elif verbosity >= 2:
    logging.basicConfig(level=logging.DEBUG)
  else:
    logging.basicConfig(level=logging.WARNING)

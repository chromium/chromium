# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script provides the necessary flags to symbolize proto traces.
"""

import os
import sys
import optparse
import logging
import webbrowser
import subprocess


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
      '--local_build_dir',
      help=('Path to a local build directory where symbols can be found. '
            'Ignored if not provided.'),
      dest='local_build_dir')
  symbolization_options.add_option(
      '--dump_syms_path',
      help=('Optional path to dump_syms binary. Searches the given build '
            'directory for the binary, if not provided.'),
      dest='dump_syms_path')
  symbolization_options.add_option(
      '--breakpad_output_dir',
      help=('Optional path to empty directory to store fetched trace symbols '
            'for official builds. This can be used if you need to symbolize '
            'traces from the same application multiple times. '
            'Automatically uses temporary directory if flag not specified.'),
      dest='breakpad_output_dir')
  symbolization_options.add_option(
      '--local_breakpad_dir',
      help=('Optional path to local directory with breakpad symbol files. '
            'These files will be used to symbolize a given trace file. Files '
            'should be named with the build id of the chrome build in upper '
            'case hexadecimal and a ".breakpad" suffix. '
            'Ex: <module_id>.breakpad. Assumes that no local directory exists '
            'if the flag is not specified.'),
      dest='local_breakpad_dir')
  symbolization_options.add_option(
      '--output_file',
      help=('Optional path to the file to write symbolized trace to. '
            'Defaults to trace_file + "symbolized_trace."'),
      dest='output_file')
  symbolization_options.add_option(
      '--cloud_storage_bucket',
      help=('Optional bucket in cloud storage where symbols reside. '
            'Defaults to "chrome-unsigned".'),
      dest='cloud_storage_bucket',
      default='chrome-unsigned')
  symbolization_options.add_option(
      '--symbolizer_path',
      help=('Optional path to the trace_to_text binary to be called for '
            'symbolization. Defaults to '
            '"third_party/perfetto/tools/traceconv". traceconv is the same as '
            'trace_to_text, except that tracevonv finds a prebuilt '
            'trace_to_text binary to use.'),
      dest='symbolizer_path',
      default='third_party/perfetto/tools/traceconv')

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

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script provides the necessary flags to symbolize proto traces.
"""

import logging
import optparse
import os
import sys


sys.path.insert(
    0,
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, 'third_party',
                 'catapult', 'systrace'))
from systrace import util

TRACING_LOGGER_NAME = 'chromium:tools/tracing'


def GeneralOptions(parser):
  """Build option group for general options.

  Args:
    parser: OptionParser object for parsing the command-line.

  Returns:
    Option group that contains general options.
  """
  general_options = optparse.OptionGroup(parser, 'General Options')

  general_options.add_option('-o',
                             '--output',
                             help=('Save trace output to file. '
                                   'Defaults to trace_file + '
                                   '"_symbolized_trace."'),
                             dest='output_file')
  general_options.add_option('-v',
                             '--verbose',
                             help='Increase output verbosity.',
                             action='count',
                             dest='verbosity',
                             default=0)
  general_options.add_option('--view',
                             help='Open resulting trace file in a '
                             'browser.',
                             action='store_true',
                             dest='view')

  return general_options


def ProfileOptions(parser):
  """Build option group for profiling chrome.

  Args:
    parser: OptionParser object for parsing the command-line.

  Returns:
    Option group that contains profiling chrome options.
  """
  profile_options = optparse.OptionGroup(parser, 'Profile Chrome Options')
  browsers = sorted(util.get_supported_browsers().keys())

  profile_options.add_option('-b',
                             '--browser',
                             help='Select among installed browsers. '
                             'One of ' + ', '.join(browsers) +
                             '. "stable" is used by '
                             'default.',
                             type='choice',
                             choices=browsers,
                             default='stable')
  profile_options.add_option('-t',
                             '--time',
                             help=('Stops tracing after N seconds. '
                                   'Default is 5 seconds'),
                             default=5,
                             metavar='N',
                             type='int',
                             dest='trace_time')
  profile_options.add_option('-e',
                             '--serial',
                             help='adb device serial number.',
                             type='string',
                             default=util.get_default_serial(),
                             dest='device_serial_number')
  profile_options.add_option('-f',
                             '--trace_format',
                             help='Format of saved trace: proto, json, html.'
                             ' Default is proto.',
                             default='proto',
                             dest='trace_format')
  profile_options.add_option('-p',
                             '--platform',
                             help='Device platform. Only Android is supported.',
                             default='android',
                             dest='platform')
  profile_options.add_option('--buf-size',
                             help='Use a trace buffer size '
                             ' of N KB.',
                             type='int',
                             metavar='N',
                             dest='trace_buf_size')
  profile_options.add_option(
      '--enable_profiler',
      help='Comma-separated string of '
      'profiling options to use. Supports options for memory or '
      'cpu or both. Ex: --enable_profiler=memory '
      'or --enable_profiler=memory,cpu. ',
      dest='enable_profiler')
  profile_options.add_option('--chrome_categories',
                             help='Chrome tracing '
                             'categories to record.',
                             type='string')
  profile_options.add_option(
      '--skip_symbolize',
      help='Skips symbolization after recording trace profile, if specified.',
      action='store_true',
      dest='skip_symbolize')
  profile_options.add_option('--compress',
                             help='Compress the resulting trace '
                             'with gzip. ',
                             action='store_true')

  # This is kept for backwards compatibility. Help is suppressed because this
  # should be specified through the newer |trace_format| flag.
  profile_options.add_option('--json',
                             help=optparse.SUPPRESS_HELP,
                             dest='write_json')

  return profile_options


def SymbolizeOptions(parser):
  """Build option group for proto trace symbolization.

  Args:
    parser: OptionParser object for parsing the command-line.

  Returns:
    Option group that contains symbolization options.
  """
  symbolization_options = optparse.OptionGroup(parser, 'Symbolization Options')

  symbolization_options.add_option(
      '--breakpad_output_dir',
      help='Optional path to empty directory to store fetched trace symbols '
      'and extracted breakpad symbol files for official builds. Useful '
      'for symbolizing multiple traces from the same Chrome build. Use '
      '--local_breakpad_dir when you already have breakpad files fetched. '
      'Automatically uses temporary directory if flag not specified.',
      dest='breakpad_output_dir')
  symbolization_options.add_option(
      '--local_breakpad_dir',
      help='Optional path to local directory containing breakpad symbol files '
      'used to symbolize the given trace. Breakpad files should be named '
      'with module id in upper case hexadecimal and ".breakpad" '
      'suffix. Ex: 12EFA32BE.breakpad',
      dest='local_breakpad_dir')
  symbolization_options.add_option(
      '--local_build_dir',
      help='Optional path to a local build directory containing symbol files.',
      dest='local_build_dir')
  symbolization_options.add_option(
      '--dump_syms',
      help=('Path to a dump_syms binary. Required when symbolizing an official '
            'build. Optional on local builds as we can use --local_build_dir '
            'to locate a binary. If built locally with '
            'autoninja -C out/Release dump_syms then the binary path is '
            'out/Release/dump_syms'),
      dest='dump_syms_path')
  symbolization_options.add_option(
      '--symbolizer',
      help=('Path to the trace_to_text binary for symbolization. Defaults to '
            '"third_party/perfetto/tools/traceconv". traceconv is the same as '
            'trace_to_text, except that tracevonv finds a prebuilt '
            'trace_to_text binary to use.'),
      default='third_party/perfetto/tools/traceconv',
      dest='symbolizer_path')
  symbolization_options.add_option(
      '--trace_processor',
      help=('Optional path to trace processor binary. '
            'Automatically downloads binary if flag not specified.'),
      dest='trace_processor_path')
  symbolization_options.add_option(
      '--cloud_storage_bucket',
      help=('Optional cloud storage bucket to where symbol files reside. '
            'Defaults to "chrome-unsigned".'),
      default='chrome-unsigned',
      dest='cloud_storage_bucket')

  return symbolization_options


def SetupProfilingCategories(options):
  """Sets --chrome_categories flag.

  Uses the --enable_profile flag to modify the --chrome_categories flag for
  specific profiling options.

  Args:
    options: The command-line options given.
  """
  if options.enable_profiler is None:
    if not options.chrome_categories:
      GetTracingLogger().warning(
          'No trace category or profiler is enabled using --enable_profiler '
          'and/or --chrome_categories, enabling all default categories.')
    if not options.skip_symbolize:
      GetTracingLogger().warning(
          'No profiler is enabled, symbolization might fail without any '
          'frames. Use --skip_symbolize to disable symbolization.')
    else:
      GetTracingLogger().info(
          'No profiler is enabled, the trace will only have trace events.')
    return

  if options.chrome_categories is None:
    options.chrome_categories = ''
  profile_options = options.enable_profiler.split(',')
  # Add to the --chrome_categories flag.
  if 'cpu' in profile_options:
    if options.chrome_categories:
      options.chrome_categories += ','
    options.chrome_categories += 'disabled-by-default-cpu_profiler'
  if 'memory' in profile_options:
    if options.chrome_categories:
      options.chrome_categories += ',disabled-by-default-memory-infra'
    else:
      # If heap profiling is enabled and no categories are provided, it is
      # usually not helpful to include other information in traces. '-*'
      # disables other default categories so that only memory data is recorded.
      options.chrome_categories += 'disabled-by-default-memory-infra,-*'


def GetTracingLogger():
  """Returns the logger for tools/tracing."""
  return logging.getLogger(TRACING_LOGGER_NAME)


def SetupLogging(verbosity):
  """Setup logging levels.

  Sets default level to warning.

  Args:
    verbosity: None or integer type that specifies the logging level.
      (options.verbosity)
  """
  logger = GetTracingLogger()
  if verbosity == 1:
    logging.basicConfig(level=logging.INFO)
    logger.setLevel(level=logging.INFO)
  elif verbosity >= 2:
    logging.basicConfig(level=logging.DEBUG)
    logger.setLevel(level=logging.DEBUG)
  else:
    logging.basicConfig(level=logging.WARNING)
    logger.setLevel(level=logging.WARNING)

#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Collect, archive, and analyze Chrome's binary size."""

import argparse
import atexit
import collections
import distutils.spawn
import logging
import platform
import resource
import sys

import archive
import console
import diff
import file_format
import html_report


def _LogPeakRamUsage():
  peak_ram_usage = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
  peak_ram_usage += resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
  logging.info('Peak RAM usage was %d MB.', peak_ram_usage // 1024)


def _AddCommonArguments(parser):
  parser.add_argument('-v',
                      '--verbose',
                      default=0,
                      action='count',
                      help='Verbose level (multiple times for more)')


class _DiffAction(object):
  @staticmethod
  def AddArguments(parser):
    parser.add_argument('before', help='Before-patch .size file.')
    parser.add_argument('after', help='After-patch .size file.')
    parser.add_argument('--all', action='store_true', help='Verbose diff')

  @staticmethod
  def Run(args, on_config_error):
    args.output_directory = None
    args.tool_prefix = None
    args.inputs = [args.before, args.after]
    args.query = '\n'.join([
        'd = Diff()',
        'sis = canned_queries.StaticInitializers(d.symbols)',
        'count = sis.CountsByDiffStatus()[models.DIFF_STATUS_ADDED]',
        'count += sis.CountsByDiffStatus()[models.DIFF_STATUS_REMOVED]',
        'if count > 0:',
        '  print("Static Initializers Diff:")',
        '  Print(sis, summarize=False)',
        '  print()',
        '  print("Full diff:")',
        'Print(d, verbose=%s)' % bool(args.all),
    ])
    console.Run(args, on_config_error)


class _SaveDiffAction(object):

  @staticmethod
  def AddArguments(parser):
    parser.add_argument('before', help='Before-patch .size file.')
    parser.add_argument('after', help='After-patch .size file.')
    parser.add_argument(
        'output_file',
        help='Write generated data to the specified .sizediff file.')

  @staticmethod
  def Run(args, on_config_error):
    if not args.before.endswith('.size'):
      on_config_error('Before input must end with ".size"')
    if not args.after.endswith('.size'):
      on_config_error('After input must end with ".size"')
    if not args.output_file.endswith('.sizediff'):
      on_config_error('Output must end with ".sizediff"')

    before_size_info = archive.LoadAndPostProcessSizeInfo(args.before)
    after_size_info = archive.LoadAndPostProcessSizeInfo(args.after)
    delta_size_info = diff.Diff(before_size_info, after_size_info)

    file_format.SaveDeltaSizeInfo(delta_size_info, args.output_file)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  sub_parsers = parser.add_subparsers()
  actions = collections.OrderedDict()
  actions['archive'] = (archive, 'Create a .size file')
  actions['html_report'] = (
      html_report, 'Create a stand-alone report from a .size file.')
  actions['console'] = (
      console,
      'Starts an interactive Python console for analyzing .size files.')
  actions['diff'] = (
      _DiffAction(),
      'Shorthand for console --query "Print(Diff())" (plus highlights static '
      'initializers in diff)')
  actions['save_diff'] = (
      _SaveDiffAction(),
      'Create a stand-alone .sizediff diff report from two .size files.')

  for name, tup in actions.items():
    sub_parser = sub_parsers.add_parser(name, help=tup[1])
    _AddCommonArguments(sub_parser)
    tup[0].AddArguments(sub_parser)
    sub_parser.set_defaults(func=tup[0].Run)

  # Show help if the command or a subcommand is called with no arguments
  if len(sys.argv) == 1:
    parser.print_help()
    sys.exit(1)
  elif len(sys.argv) == 2 and sys.argv[1] in actions:
    parser.parse_args(sys.argv[1:] + ['-h'])
    sys.exit(1)

  args = parser.parse_args()
  logging.basicConfig(level=logging.WARNING - args.verbose * 10,
                      format='%(levelname).1s %(relativeCreated)6d %(message)s')

  if logging.getLogger().isEnabledFor(logging.DEBUG):
    atexit.register(_LogPeakRamUsage)

  def on_config_error(*args):
    parser.error(*args)

  args.func(args, on_config_error)


if __name__ == '__main__':
  main()

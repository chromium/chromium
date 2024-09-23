#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Collect, archive, and analyze Chrome's binary size."""

import argparse
import atexit
import logging
import pathlib
import platform
import resource
import sys

import archive
import console
import diff
import dex_disassembly
import file_format
import models
import native_disassembly
import os


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


class _PathResolver:
  def __init__(self, parent_path):
    self._parent_path = pathlib.Path(parent_path)

  def __call__(self, subpath):
    # Use dict to de-dupe while keeping order.
    candidates = list({
        self._parent_path / subpath: 0,
        self._parent_path / pathlib.PosixPath(subpath).name: 0,
    })
    for p in candidates:
      if p.exists():
        return p
    raise Exception('Paths do not exist: ' +
                    ', '.join(str(t) for t in candidates))


class _DiffAction:
  @staticmethod
  def AddArguments(parser):
    parser.add_argument('before', help='Before-patch .size file.')
    parser.add_argument('after', help='After-patch .size file.')
    parser.add_argument('--all', action='store_true', help='Verbose diff')

  @staticmethod
  def Run(args, on_config_error):
    args.output_directory = None
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


class _SaveDiffAction:
  @staticmethod
  def AddArguments(parser):
    parser.add_argument('before', help='Before-patch .size file.')
    parser.add_argument('after', help='After-patch .size file.')
    parser.add_argument(
        'output_file',
        help='Write generated data to the specified .sizediff file.')
    parser.add_argument('--title',
                        help='Value for the "title" build_config entry.')
    parser.add_argument('--url', help='Value for the "url" build_config entry.')
    parser.add_argument(
        '--save-disassembly',
        help='Adds the disassembly for the top 10 changed symbols.',
        action='store_true')
    parser.add_argument(
        '--before-directory',
        help='Defaults to directory containing before-patch .size file.')
    parser.add_argument(
        '--after-directory',
        help='Defaults to directory containing after-patch .size file.')

  @staticmethod
  def Run(args, on_config_error):
    if not args.before.endswith('.size'):
      on_config_error('Before input must end with ".size"')
    if not args.after.endswith('.size'):
      on_config_error('After input must end with ".size"')
    if not args.output_file.endswith('.sizediff'):
      on_config_error('Output must end with ".sizediff"')
    if args.save_disassembly:
      if not args.before_directory:
        args.before_directory = os.path.dirname(args.before)
      if not args.after_directory:
        args.after_directory = os.path.dirname(args.after)
    before_size_info = archive.LoadAndPostProcessSizeInfo(args.before)
    after_size_info = archive.LoadAndPostProcessSizeInfo(args.after)
    # If a URL or title exists, we only want to add it to the build config of
    # the after size file.
    if args.title:
      after_size_info.build_config[models.BUILD_CONFIG_TITLE] = args.title
    if args.url:
      after_size_info.build_config[models.BUILD_CONFIG_URL] = args.url
    delta_size_info = diff.Diff(before_size_info, after_size_info)
    if args.save_disassembly:
      before_path_resolver = _PathResolver(args.before_directory)
      after_path_resolver = _PathResolver(args.after_directory)
      dex_disassembly.AddDisassembly(delta_size_info, before_path_resolver,
                                     after_path_resolver)
      native_disassembly.AddDisassembly(delta_size_info, before_path_resolver,
                                        after_path_resolver)

    file_format.SaveDeltaSizeInfo(delta_size_info, args.output_file)


def main():
  parser = argparse.ArgumentParser(prog='supersize', description=__doc__)
  sub_parsers = parser.add_subparsers()
  actions = {}
  actions['archive'] = (archive, 'Create a .size file')
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

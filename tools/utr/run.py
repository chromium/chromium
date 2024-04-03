#!/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Command-line interface of the UTR

Using a specified builder name, this tool can build and/or launch a test the
same way it's done on the bots. See the README.md in //tools/utr/ for more info.
"""

import argparse
import logging
import os
import pathlib
import sys

import builders
import cipd
import recipe

from rich.logging import RichHandler

_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_DIR = _THIS_DIR.parents[1]


def add_common_args(parser):
  parser.add_argument('--verbose',
                      '-v',
                      dest='verbosity',
                      default=0,
                      action='count',
                      help='Enable additional runtime logging. Pass multiple '
                      'times for increased logging.')
  parser.add_argument('--force',
                      '-f',
                      action='store_true',
                      help='Skip all prompts about config mismatches.')
  parser.add_argument('--test',
                      '-t',
                      required=True,
                      action='append',
                      default=[],
                      dest='tests',
                      help='Name of test suite(s) to replicate. Pass multiple '
                      'times for multiple tests.')
  parser.add_argument('--builder',
                      '-b',
                      required=True,
                      help='Name of the builder we want to replicate.')
  parser.add_argument(
      '--bucket',
      '-B',
      help='Name of the bucket of the builder. Will attempt to automatically '
      'determine if not specified.')
  parser.add_argument(
      '--build-dir',
      '--out-dir',
      '-o',
      type=pathlib.Path,
      help='Path to the build dir to use for compilation and/or for invoking '
      'test binaries. Will use the output path used by the builder if not '
      'specified (likely //out/Release/).')
  parser.add_argument(
      '--recipe-path',
      '-r',
      type=pathlib.Path,
      help='Path to override the recipe bundle with a local checkout.')


def add_compile_args(parser):
  parser.add_argument(
      '--no-rbe',
      action='store_true',
      help='Disables the use of rbe ("use_remoteexec" GN arg) in the compile. '
      "Will use the builder's settings if not specified.")
  parser.add_argument(
      '--no-siso',
      action='store_true',
      help='Disables the use of siso ("use_siso" GN arg) in the compile. '
      "Will use the builder's settings if not specified.")


def parse_args(args=None):
  """Parse cmd line args.

  Args:
    args: Cmd line args to parse. Only passed in unittests. Otherwise uses argv.
  Returns:
    An argparse.ArgumentParser.
  """
  parser = argparse.ArgumentParser(description=__doc__)
  add_common_args(parser)
  subparsers = parser.add_subparsers(dest='run_mode')

  compile_subp = subparsers.add_parser(
      'compile', help='Only compiles. WARNING: this mode is not yet supported.')
  add_compile_args(compile_subp)

  subparsers.add_parser(
      'test',
      help='Only run/trigger tests. WARNING: this mode is not yet supported.')

  compile_and_test_subp = subparsers.add_parser(
      'compile-and-test',
      help='Both compile and run/trigger tests. WARNING: this mode is not yet '
      'supported.')
  add_compile_args(compile_and_test_subp)

  return parser.parse_args(args)


def main():
  args = parse_args()
  logging.basicConfig(level=logging.DEBUG if args.verbosity else logging.INFO,
                      format='%(message)s',
                      handlers=[
                          RichHandler(show_time=False,
                                      show_level=False,
                                      show_path=False,
                                      markup=True)
                      ])

  cipd_bin_path = _SRC_DIR.joinpath('third_party', 'depot_tools', '.cipd_bin')
  if not cipd_bin_path.exists():
    logging.warning(
        ".cipd_bin folder not found. 'gclient sync' may need to be run")
  else:
    os.environ["PATH"] = str(cipd_bin_path) + os.pathsep + os.environ["PATH"]

  if not recipe.check_rdb_auth():
    return 1

  if not args.recipe_path:
    recipes_path = cipd.fetch_recipe_bundle(args.verbosity).joinpath('recipes')
  else:
    recipes_path = args.recipe_path.joinpath('recipes', 'recipes.py')

  builder_props, swarming_server = builders.find_builder_props(
      args.bucket, args.builder)
  if not builder_props:
    return 1

  skip_compile = args.run_mode == 'test'
  skip_test = args.run_mode == 'compile'
  recipe_runner = recipe.LegacyRunner(
      recipes_path,
      builder_props,
      args.bucket,
      args.builder,
      swarming_server,
      args.tests,
      skip_compile,
      skip_test,
      args.force,
      args.build_dir,
  )
  exit_code, error_msg = recipe_runner.run_recipe(
      filter_stdout=args.verbosity < 2)
  if error_msg:
    logging.error('\nUTR failure:')
    logging.error(error_msg)
  return exit_code


if __name__ == '__main__':
  sys.exit(main())

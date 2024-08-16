#!/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Command-line interface of the UTR

Using a specified builder name, this tool can build and/or launch a test the
same way it's done on the bots. See the README.md in //tools/utr/ for more info.

Any additional args passed at the end of the invocation will be passed down
as-is to all triggered tests. Example uses:

- vpython3 run.py -B $BUCKET -b $BUILDER -t $TEST compile
- vpython3 run.py -B $BUCKET -b $BUILDER -t $TEST compile-and-test
- vpython3 run.py -B $BUCKET -b $BUILDER -t $TEST test --gtest_filter=Test.Case
"""

import argparse
import logging
import os
import pathlib
import re
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
                      action='append',
                      default=[],
                      dest='tests',
                      help='Name of test suite(s) to replicate. Pass multiple '
                      'times for multiple tests. Optional with the "compile" '
                      'run mode which will compile "all".')
  parser.add_argument('--builder',
                      '-b',
                      required=True,
                      help='Name of the builder we want to replicate.')
  parser.add_argument(
      '--project',
      '-p',
      help="Name of the project of the builder. Note: if you're on a release "
      'branch, you can exclude the milestone part of the name (eg: you can '
      'pass "chrome" instead of "chrome-m123"). Will attempt to automatically '
      'determine if not specified.')
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
      '--recipe-dir',
      '--recipe-path',
      '-r',
      type=pathlib.Path,
      help='Path to override the recipe bundle with a local bundle. To create '
      'a bundle locally, run `./recipes.py bundle` in your desired recipe '
      'checkout. This creates a dir called "bundle" that can be pointed to '
      'with this arg.')
  parser.add_argument('--reuse-task',
                      type=str,
                      help='Ruse the cas digest of the provided swarming task')


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
  parser.add_argument(
      '--no-coverage-instrumentation',
      action='store_true',
      help='Skips instrumenting code-coverage, even if the builder is '
      'configured to instrument. Instrumentation can inflate both build sizes '
      "and runtimes. But some failures may only occur when it's enabled.")


def add_test_args(parser):
  parser.add_argument(
      'additional_test_args',
      nargs='*',
      help='The args listed here will be appended to the test cmd-lines.')


def parse_args(args=None):
  """Parse cmd line args.

  Args:
    args: Cmd line args to parse. Only passed in unittests. Otherwise uses argv.
  Returns:
    An argparse.ArgumentParser.
  """
  parser = argparse.ArgumentParser(
      description=__doc__,
      # Custom formatter to preserve line breaks in the docstring
      formatter_class=argparse.RawDescriptionHelpFormatter)
  add_common_args(parser)
  subparsers = parser.add_subparsers(dest='run_mode')

  compile_subp = subparsers.add_parser(
      'compile',
      aliases=['build'],
      help='Only compiles. WARNING: this mode is not yet supported.')
  add_compile_args(compile_subp)

  test_subp = subparsers.add_parser(
      'test',
      help='Only run/trigger tests. WARNING: this mode is not yet supported.')
  add_test_args(test_subp)

  compile_and_test_subp = subparsers.add_parser(
      'compile-and-test',
      aliases=['build-and-test', 'run'],
      help='Both compile and run/trigger tests. WARNING: this mode is not yet '
      'supported.')
  add_compile_args(compile_and_test_subp)
  add_test_args(compile_and_test_subp)

  rr_subp = subparsers.add_parser(
      'rr',
      aliases=['rr-record', 'record'],
      help='Compile, run tests with rr tool and upload recorded traces. '
      'WARNING: this mode is not yet supported.')
  add_compile_args(rr_subp)
  add_test_args(rr_subp)

  args = parser.parse_args(args)
  if not args.run_mode:
    parser.print_help()
    parser.error('Please select a run_mode: compile,test,compile-and-test')
  if args.run_mode == 'rr':
    parser.print_help()
    parser.error('The rr mode is not yet supported in UTR')
  if args.reuse_task and args.run_mode != 'test':
    parser.print_help()
    parser.error('reuse-task is only compatible with "test"')
  if not args.tests:
    # Only compile mode should default to compile all
    if args.run_mode != 'compile':
      parser.print_help()
      parser.error('Please provide a test to run')
  if args.project:
    if re.fullmatch(r'chromium(-m\d+)?', args.project):
      args.project = 'chromium'
    elif re.fullmatch(r'chrome(-m\d+)?', args.project):
      args.project = 'chrome'
    else:
      parser.error(
          f'Unknown project: "{args.project}". Please select "chrome" or '
          '"chromium".')
  return args


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

  if not recipe.check_luci_context_auth():
    return 1

  builder_props, project = builders.find_builder_props(
      args.builder, bucket_name=args.bucket, project_name=args.project)
  if not builder_props:
    return 1

  if not args.recipe_dir:
    recipes_path = cipd.fetch_recipe_bundle(project,
                                            args.verbosity).joinpath('recipes')
  else:
    recipes_path = args.recipe_dir.joinpath('recipes')

  skip_compile = args.run_mode == 'test'
  skip_test = args.run_mode == 'compile'
  recipe_runner = recipe.LegacyRunner(
      recipes_path,
      builder_props,
      project,
      args.bucket,
      args.builder,
      args.tests,
      skip_compile,
      skip_test,
      args.force,
      args.build_dir,
      additional_test_args=None if skip_test else args.additional_test_args,
      reuse_task=args.reuse_task,
      skip_coverage=not skip_compile and args.no_coverage_instrumentation,
  )
  exit_code, error_msg = recipe_runner.run_recipe(
      filter_stdout=args.verbosity < 2)
  if error_msg:
    logging.error('\nUTR failure:')
    logging.error(error_msg)
  return exit_code


if __name__ == '__main__':
  sys.exit(main())

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
import sys

import builders
import cipd
import recipe


def add_common_args(parser):
  parser.add_argument('--verbose',
                      '-v',
                      action='store_true',
                      help='Enable additional runtime logging.')
  parser.add_argument('--test',
                      '-t',
                      nargs='+',
                      required=True,
                      help='Name of test suite(s) to replicate.')
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
      help='Path to the build dir to use for compilation and/or for invoking '
      'test binaries. Will use the output path used by the builder if not '
      'specified (likely //out/Release/).')


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


def parse_args():
  parser = argparse.ArgumentParser(description=__doc__)
  add_common_args(parser)
  subparsers = parser.add_subparsers(dest='run_mode')

  compile_subp = subparsers.add_parser(
      'compile', help='Only compiles. WARNING: this mode is not yet supported.')
  add_compile_args(compile_subp)

  test_subp = subparsers.add_parser(
      'test',
      help='Only run/trigger tests. WARNING: this mode is not yet supported.')

  compile_and_test_subp = subparsers.add_parser(
      'compile-and-test',
      help='Both compile and run/trigger tests. WARNING: this mode is not yet '
      'supported.')
  add_compile_args(compile_and_test_subp)

  return parser.parse_args()


def main():
  args = parse_args()
  logging.basicConfig(level=logging.DEBUG if args.verbose else logging.WARN)

  bundle_root = cipd.fetch_recipe_bundle(args.verbose)
  builder_props = builders.find_builder_props(args.bucket, args.builder)
  if not builder_props:
    return 1

  skip_compile = args.run_mode == 'test'
  skip_test = args.run_mode == 'compile'
  recipe_runner = recipe.LegacyRunner(
      bundle_root,
      builder_props,
      args.bucket,
      args.builder,
      args.test,
      skip_compile,
      skip_test,
      args.build_dir,
  )
  # TODO(crbug.com/41492688): Add a more rich channel of output from the UTR
  # rather than simply exit code, and use that to send Y/N prompts to the user
  # to control/override default behavior (eg: using pre-builts with incorrect
  # GN args).
  return recipe_runner.run_recipe()


if __name__ == '__main__':
  sys.exit(main())

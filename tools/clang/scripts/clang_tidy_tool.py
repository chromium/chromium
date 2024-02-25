#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
r"""Automatically fetch, build, and run clang-tidy from source.

This script seeks to automate the steps detailed in docs/clang_tidy.md.

Example: the following command disables clang-tidy's default checks (-*) and
enables the clang static analyzer checks.

    tools/clang/scripts/clang_tidy_tool.py \\
       --checks='-*,clang-analyzer-*,-clang-analyzer-alpha*' \\
       --header-filter='.*' \\
       out/Release chrome

The same, but checks the changes only.

    git diff -U5 | tools/clang/scripts/clang_tidy_tool.py \\
       --diff \\
       --checks='-*,clang-analyzer-*,-clang-analyzer-alpha*' \\
       --header-filter='.*' \\
       out/Release chrome
"""

from __future__ import print_function

import argparse
import os
import subprocess
import sys
import update

import build_clang_tools_extra


def GetBinaryPath(build_dir, binary):
  if sys.platform == 'win32':
    binary += '.exe'
  return os.path.join(build_dir, 'bin', binary)


def BuildNinjaTarget(out_dir, ninja_target):
  args = ['autoninja', '-C', out_dir, ninja_target]
  subprocess.check_call(args, shell=sys.platform == 'win32')


def GenerateCompDb(out_dir):
  gen_compdb_script = os.path.join(
      os.path.dirname(__file__), 'generate_compdb.py')
  comp_db_file_path = os.path.join(out_dir, 'compile_commands.json')
  args = [
      sys.executable,
      gen_compdb_script,
      '-p',
      out_dir,
      '-o',
      comp_db_file_path,
  ]
  subprocess.check_call(args)

  # The resulting CompDb file includes /showIncludes which causes clang-tidy to
  # output a lot of unnecessary text to the console.
  with open(comp_db_file_path, 'r') as comp_db_file:
    comp_db_data = comp_db_file.read();

  # The trailing space on /showIncludes helps keep single-spaced flags.
  comp_db_data = comp_db_data.replace('/showIncludes ', '')

  with open(comp_db_file_path, 'w') as comp_db_file:
    comp_db_file.write(comp_db_data)


def RunClangTidy(checks, header_filter, auto_fix, clang_src_dir,
                 clang_build_dir, out_dir, ninja_target):
  """Invoke the |run-clang-tidy.py| script."""
  run_clang_tidy_script = os.path.join(
      clang_src_dir, 'clang-tools-extra', 'clang-tidy', 'tool',
      'run-clang-tidy.py')

  clang_tidy_binary = GetBinaryPath(clang_build_dir, 'clang-tidy')
  clang_apply_rep_binary = GetBinaryPath(clang_build_dir,
                                         'clang-apply-replacements')

  args = [
      sys.executable,
      run_clang_tidy_script,
      '-quiet',
      '-p',
      out_dir,
      '-clang-tidy-binary',
      clang_tidy_binary,
      '-clang-apply-replacements-binary',
      clang_apply_rep_binary,
  ]

  if checks:
    args.append('-checks={}'.format(checks))

  if header_filter:
    args.append('-header-filter={}'.format(header_filter))

  if auto_fix:
    args.append('-fix')

  args.append(ninja_target)
  subprocess.check_call(args)


def RunClangTidyDiff(checks, auto_fix, clang_src_dir, clang_build_dir, out_dir):
  """Invoke the |clang-tidy-diff.py| script over the diff from stdin."""
  clang_tidy_diff_script = os.path.join(
      clang_src_dir, 'clang-tools-extra', 'clang-tidy', 'tool',
      'clang-tidy-diff.py')

  clang_tidy_binary = GetBinaryPath(clang_build_dir, 'clang-tidy')

  args = [
      clang_tidy_diff_script,
      '-quiet',
      '-p1',
      '-path',
      out_dir,
      '-clang-tidy-binary',
      clang_tidy_binary,
  ]

  if checks:
    args.append('-checks={}'.format(checks))

  if auto_fix:
    args.append('-fix')

  subprocess.check_call(args)


def main():
  script_name = sys.argv[0]

  parser = argparse.ArgumentParser(
      formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
  parser.add_argument(
      '--fetch',
      nargs='?',
      const=update.CLANG_REVISION,
      help='Fetch and build clang sources')
  parser.add_argument(
      '--build',
      action='store_true',
      help='build clang sources to get clang-tidy')
  parser.add_argument(
      '--diff',
      action='store_true',
      default=False,
      help ='read diff from the stdin and check it')
  parser.add_argument('--clang-src-dir', type=str,
                      help='override llvm and clang checkout location')
  parser.add_argument('--clang-build-dir', type=str,
                      help='override clang build dir location')
  parser.add_argument('--checks', help='passed to clang-tidy')
  parser.add_argument('--header-filter', help='passed to clang-tidy')
  parser.add_argument(
      '--auto-fix',
      action='store_true',
      help='tell clang-tidy to auto-fix errors')
  parser.add_argument('OUT_DIR', help='where we are building Chrome')
  parser.add_argument('NINJA_TARGET', help='ninja target')
  args = parser.parse_args()

  steps = []

  # If the user hasn't provided a clang checkout and build dir, checkout and
  # build clang-tidy where update.py would.
  if not args.clang_src_dir:
    args.clang_src_dir = build_clang_tools_extra.GetCheckoutDir(args.OUT_DIR)
  if not args.clang_build_dir:
    args.clang_build_dir = build_clang_tools_extra.GetBuildDir(args.OUT_DIR)
  elif (args.clang_build_dir and not
        os.path.isfile(GetBinaryPath(args.clang_build_dir, 'clang-tidy'))):
    sys.exit('clang-tidy binary doesn\'t exist at ' +
             GetBinaryPath(args.clang_build_dir, 'clang-tidy'))

  if args.fetch:
    steps.append(('Fetching LLVM sources', lambda:
                  build_clang_tools_extra.FetchLLVM(args.clang_src_dir,
                                                    args.fetch)))

  if args.build:
    steps.append(('Building clang-tidy',
                  lambda: build_clang_tools_extra.BuildTargets(
                      args.clang_build_dir,
                      ['clang-tidy', 'clang-apply-replacements'])))

  steps += [
      ('Building ninja target: %s' % args.NINJA_TARGET,
      lambda: BuildNinjaTarget(args.OUT_DIR, args.NINJA_TARGET)),
      ('Generating compilation DB', lambda: GenerateCompDb(args.OUT_DIR))
    ]
  if args.diff:
    steps += [
        ('Running clang-tidy on diff', lambda: RunClangTidyDiff(
            args.checks, args.auto_fix, args.clang_src_dir, args.
            clang_build_dir, args.OUT_DIR)),
    ]
  else:
    steps += [
        ('Running clang-tidy',
        lambda: RunClangTidy(args.checks, args.header_filter,
                             args.auto_fix, args.clang_src_dir,
                             args.clang_build_dir, args.OUT_DIR,
                             args.NINJA_TARGET)),
    ]

  # Run the steps in sequence.
  for i, (msg, step_func) in enumerate(steps):
    # Print progress message
    print('-- %s %s' % (script_name, '-' * (80 - len(script_name) - 4)))
    print('-- [%d/%d] %s' % (i + 1, len(steps), msg))
    print(80 * '-')

    step_func()

  return 0


if __name__ == '__main__':
  sys.exit(main())

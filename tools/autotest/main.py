# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Builds and runs a test by filename.

This script finds the appropriate test suites for the specified test files,
directories, or test names, builds it, then runs it with the (optionally)
specified filter, passing any extra args on to the test runner.

Examples:
# Run the test target for bit_cast_unittest.cc. Use a custom test filter instead
# of the automatically generated one.
autotest.py -C out/Desktop bit_cast_unittest.cc --gtest_filter=BitCastTest*

# Find and run UrlUtilitiesUnitTest.java's tests, pass remaining parameters to
# the test binary.
autotest.py -C out/Android UrlUtilitiesUnitTest --fast-local-dev -v

# Run all tests under base/strings.
autotest.py -C out/foo --run-all base/strings

# Run tests in multiple files or directories.
autotest.py -C out/foo base/strings base/pickle_unittest.cc

# Run only the test on line 11. Useful when running autotest.py from your text
# editor.
autotest.py -C out/foo --line 11 base/strings/strcat_unittest.cc

# Search for and run tests with the given names.
autotest.py -C out/foo StringUtilTest.IsStringUTF8 SpanTest.AsStringView
"""

import argparse
import os
import re
import sys
import shutil

import test_executor
import finders.file_finder as file_finder
import finders.target_finder as target_finder
import utils.command_util as command
import utils.constants as const
import utils.telemetry as telemetry
from utils.command_error import AutotestError, CommandError

from pathlib import Path

sys.path.append(str(const.SRC_DIR / 'build' / 'android'))
from pylib import constants


def BuildCppTestFilter(filenames: list[str], line: int | None) -> str:
  make_filter_command: list[str | Path] = [
      sys.executable, const.SRC_DIR / 'tools' / 'make_gtest_filter.py'
  ]
  if line:
    make_filter_command += ['--line', str(line)]
  else:
    make_filter_command += ['--class-only']
  make_filter_command += filenames
  return command.RunCommand(make_filter_command).strip()


def BuildJavaTestFilter(filenames: list[str]) -> str:
  return ':'.join('*.{}*'.format(os.path.splitext(os.path.basename(f))[0])
                  for f in filenames)


_PREF_MAPPING_GTEST_FILTER: str = '*PolicyPrefsTest.PolicyToPrefsMapping*'

_PREF_MAPPING_FILE_REGEX: re.Pattern[str] = re.compile(
    const.PREF_MAPPING_FILE_PATTERN)

SPECIAL_TEST_FILTERS: list[tuple[re.Pattern[str], str]] = [
    (_PREF_MAPPING_FILE_REGEX, _PREF_MAPPING_GTEST_FILTER)
]


def BuildTestFilter(filenames: list[str], line: int | None) -> str:
  java_files: list[str] = [f for f in filenames if f.endswith('.java')]
  # TODO(crbug.com/434009870): Support EarlGrey tests, which don't use
  # Googletest's macros or pascal case naming convention.
  cc_files: list[str] = [
      f for f in filenames if f.endswith('.cc') or f.endswith('_unittest.mm')
  ]
  filters: list[str] = []
  if java_files:
    filters.append(BuildJavaTestFilter(java_files))
  if cc_files:
    filters.append(BuildCppTestFilter(cc_files, line))
  for regex, gtest_filter in SPECIAL_TEST_FILTERS:
    if any(True for f in filenames if regex.match(f)):
      filters.append(gtest_filter)
      break
  return ':'.join(filters)


def BuildPrefMappingTestFilter(filenames: list[str]) -> str | None:
  mapping_files: list[str] = [
      f for f in filenames if _PREF_MAPPING_FILE_REGEX.match(f)
  ]
  if not mapping_files:
    return None
  names_without_extension: list[str] = [Path(f).stem for f in mapping_files]
  return ':'.join(names_without_extension)


@telemetry.tracer.start_as_current_span('chromium.tools.autotest.main')
def main() -> int:
  parser: argparse.ArgumentParser = argparse.ArgumentParser(
      prog='tools/autotest.py',
      description=__doc__,
      formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('--out-dir',
                      '--out_dir',
                      '--output-directory',
                      '--output_directory',
                      '-C',
                      metavar='OUT_DIR',
                      help='output directory of the build')
  parser.add_argument('--remote-search',
                      '--remote_search',
                      '-r',
                      action='store_true',
                      help='Search for tests using a remote service')
  parser.add_argument('--name',
                      action='append',
                      help='Search for the test by name, and apply test filter')
  parser.add_argument(
      '--run-all',
      '--run_all',
      action='store_true',
      help='Run all tests for the file or directory, instead of just one')
  parser.add_argument(
      '--target-index',
      '--target_index',
      type=int,
      help='When the target is ambiguous, choose the one with this index.')
  parser.add_argument(
      '--path-index',
      '--path_index',
      type=int,
      help='When the test path is ambiguous, choose the one with this index.')
  parser.add_argument(
      '--run-changed',
      '--run_changed',
      action='store_true',
      help='Run tests files modified since this branch diverged from main.')
  parser.add_argument('--line',
                      type=int,
                      help='run only the test on this line number. c++ only.')
  parser.add_argument('--gtest-filter',
                      '--gtest_filter',
                      '-f',
                      metavar='FILTER',
                      help='test filter')
  parser.add_argument('--test-policy-to-pref-mappings-filter',
                      '--test_policy_to_pref_mappings_filter',
                      metavar='FILTER',
                      help='policy pref mappings test filter')
  parser.add_argument(
      '--dry-run',
      '--dry_run',
      '-n',
      action='store_true',
      help='Print ninja and test run commands without executing them.')
  parser.add_argument(
      '--quiet',
      '-q',
      action='store_true',
      help='Do not print while building, only print if build fails.')
  parser.add_argument(
      '--no-try-android-wrappers',
      '--no_try_android_wrappers',
      action='store_true',
      help='Do not try to use Android test wrappers to run tests.')
  parser.add_argument('--no-fast-local-dev',
                      '--no_fast_local_dev',
                      action='store_true',
                      help='Do not add --fast-local-dev for Android tests.')
  parser.add_argument('--no-single-variant',
                      '--no_single_variant',
                      action='store_true',
                      help='Do not add --single-variant for Android tests.')
  parser.add_argument('--no-build',
                      '--no-build',
                      action='store_true',
                      help='Do not build before running tests.')
  parser.add_argument('files',
                      metavar='FILE_NAME',
                      nargs='*',
                      help='test suite file (eg. FooTest.java) or test name')

  args, _extras = parser.parse_known_args()

  if args.out_dir:
    constants.SetOutputDirectory(args.out_dir)
  constants.CheckOutputDirectory()
  out_dir: str = constants.GetOutDirectory()

  if not os.path.isdir(out_dir):
    parser.error(f'OUT_DIR "{out_dir}" does not exist.')
  target_cache: target_finder.TargetCache = target_finder.TargetCache(out_dir)

  if not args.run_changed and not args.files and not args.name:
    parser.error('Specify a file to test or use --run-changed')

  # Cog is almost unusable with local search, so turn on remote_search.
  use_remote_search: bool = args.remote_search
  if not use_remote_search and const.SRC_DIR.parts[:3] == ('/', 'google',
                                                           'cog'):
    if const.DEBUG:
      print('Detected cog, turning on remote-search.')
    use_remote_search = True

  gtest_filter: str | None = args.gtest_filter

  # Don't try to search if rg is not installed, and use the old behavior.
  if not use_remote_search and not shutil.which('rg'):
    if not args.quiet:
      print(
          'rg command not found. Install ripgrep to enable running tests by name.'
      )
    files_to_test = args.files
    test_names = []
  else:
    test_names = [f for f in args.files if not file_finder.IsProbablyFile(f)]
    files_to_test = [f for f in args.files if file_finder.IsProbablyFile(f)]

  if args.name:
    test_names.extend(args.name)
  if test_names:
    files, filter = file_finder.SearchForTestsByName(test_names, args.quiet,
                                                     use_remote_search)
    if not gtest_filter:
      gtest_filter = filter
    files_to_test.extend(files)

  if args.run_changed:
    files_to_test.extend(file_finder.GetChangedTestFiles())
    # Remove duplicates.
    files_to_test = list(set(files_to_test))

  filenames: list[str] = []
  for file in files_to_test:
    filenames.extend(
        file_finder.FindMatchingTestFiles(file, use_remote_search,
                                          args.path_index))

  if not filenames:
    command.ExitWithMessage('No associated test files found.')

  targets, used_cache = target_finder.FindTestTargets(target_cache, out_dir,
                                                      filenames, args)

  if not gtest_filter:
    gtest_filter = BuildTestFilter(filenames, args.line)

  if not gtest_filter:
    command.ExitWithMessage('Failed to derive a gtest filter')

  pref_mapping_filter: str | None = args.test_policy_to_pref_mappings_filter
  if not pref_mapping_filter:
    pref_mapping_filter = BuildPrefMappingTestFilter(filenames)

  assert targets

  if not args.no_build:
    build_ok: bool = test_executor.BuildTestTargets(out_dir, targets,
                                                    args.dry_run, args.quiet,
                                                    False)

    # If we used the target cache, it's possible we chose the wrong target
    # because a gn file was changed. The build step above will check for gn
    # modifications and update build.ninja. Use this opportunity the verify the
    # cache is still valid.
    if used_cache and not target_cache.IsStillValid():
      target_cache = target_finder.TargetCache(out_dir)
      new_targets, _ = target_finder.FindTestTargets(target_cache, out_dir,
                                                     filenames, args)
      if targets != new_targets:
        # Note that this can happen, for example, if you rename a test target.
        print('gn config was changed, trying to build again', file=sys.stderr)
        targets = new_targets
        build_ok: bool = test_executor.BuildTestTargets(out_dir, targets,
                                                        args.dry_run,
                                                        args.quiet, True)
      telemetry.RecordMainAttributes(targets, gtest_filter, used_cache, out_dir)

      if not build_ok:
        return 1

  return test_executor.RunTestTargets(out_dir, targets, gtest_filter,
                                      pref_mapping_filter, _extras,
                                      args.dry_run,
                                      args.no_try_android_wrappers,
                                      args.no_fast_local_dev,
                                      args.no_single_variant)


if __name__ == '__main__':
  telemetry.telemetry.initialize('chromium.tools.autotest')

  try:
    sys.exit(main())
  except (AutotestError, CommandError) as e:
    print(e, file=sys.stderr)
    sys.exit(1)

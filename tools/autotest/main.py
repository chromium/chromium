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

import os
import sys
import shutil
import click

import filters
import finders.file_finder as file_finder
import finders.target_finder as target_finder
import test_executor
import utils.command_util as command
import utils.constants as const
import utils.telemetry as telemetry

from utils.command_error import AutotestError, CommandError
from utils.options import AutotestConfig, Formatter, autotest_options

sys.path.append(str(const.SRC_DIR / 'build' / 'android'))
from pylib import constants


@click.command(cls=Formatter,
               help=__doc__,
               context_settings=dict(ignore_unknown_options=True,
                                     allow_interspersed_args=True,
                                     allow_extra_args=True,
                                     help_option_names=['-h', '--help']))
@autotest_options
@click.pass_context
@telemetry.tracer.start_as_current_span('chromium.tools.autotest.main')
def main(ctx, **kwargs) -> int:

  files_to_test = []
  extras = []

  parsing_files = True
  for arg in ctx.args:
    if len(files_to_test) == 0:
      parsing_files = True

    if arg.startswith('-'):
      parsing_files = False

    if parsing_files:
      files_to_test.append(arg)
    else:
      extras.append(arg)

  kwargs['files'] = tuple(files_to_test)
  kwargs['extras'] = extras

  config: AutotestConfig = AutotestConfig(**kwargs)

  if config.out_dir:
    constants.SetOutputDirectory(config.out_dir)
  constants.CheckOutputDirectory()
  out_dir = constants.GetOutDirectory()

  if not os.path.isdir(out_dir):
    raise click.UsageError(f'OUT_DIR "{out_dir}" does not exist.')

  target_cache: target_finder.TargetCache = target_finder.TargetCache(out_dir)

  if not config.run_changed and not config.files and not config.name:
    raise click.UsageError('Specify a file to test or use --run-changed')

  # Cog is almost unusable with local search, so turn on remote_search.
  use_remote_search: bool = config.remote_search
  if not use_remote_search and const.SRC_DIR.parts[:3] == ('/', 'google',
                                                           'cog'):
    if const.DEBUG:
      click.echo('Detected cog, turning on remote-search.')
    use_remote_search = True

  # Don't try to search if rg is not installed, and use the old behavior.
  if not use_remote_search and not shutil.which('rg'):
    if not config.quiet:
      click.echo(
          'rg command not found. Install ripgrep to enable running tests by name.'
      )
    files_to_test = list(config.files)
    test_names = []
  else:
    test_names = [f for f in config.files if not file_finder.IsProbablyFile(f)]
    files_to_test = [f for f in config.files if file_finder.IsProbablyFile(f)]

  if config.name:
    test_names.extend(config.name)

  current_gtest_filter: str | None = config.gtest_filter
  if test_names:
    found_files, found_filter = file_finder.SearchForTestsByName(
        test_names, config.quiet, use_remote_search)
    if not current_gtest_filter:
      current_gtest_filter = found_filter
    files_to_test.extend(found_files)

  if config.run_changed:
    files_to_test.extend(file_finder.GetChangedTestFiles())
    files_to_test = list(set(files_to_test))

  filenames: list[str] = []
  for f in files_to_test:
    filenames.extend(
        file_finder.FindMatchingTestFiles(f, use_remote_search,
                                          config.path_index))

  if not filenames:
    command.ExitWithMessage('No associated test files found.')

  targets, used_cache = target_finder.FindTestTargets(target_cache, out_dir,
                                                      filenames, config.run_all,
                                                      config.run_changed,
                                                      config.target_index)

  if not current_gtest_filter:
    current_gtest_filter = filters.BuildTestFilter(filenames, config.line)

  if not current_gtest_filter:
    command.ExitWithMessage('Failed to derive a gtest filter')

  pref_mapping_filter: str | None = config.test_policy_to_pref_mappings_filter
  if not pref_mapping_filter:
    pref_mapping_filter = filters.BuildPrefMappingTestFilter(filenames)

  assert targets

  if not config.no_build:
    build_ok: bool = test_executor.BuildTestTargets(out_dir, targets,
                                                    config.dry_run,
                                                    config.quiet, False)

    # If we used the target cache, it's possible we chose the wrong target
    # because a gn file was changed. The build step above will check for gn
    # modifications and update build.ninja. Use this opportunity the verify the
    # cache is still valid.
    if used_cache and not target_cache.IsStillValid():
      target_cache = target_finder.TargetCache(out_dir)
      new_targets, _ = target_finder.FindTestTargets(target_cache, out_dir,
                                                     filenames, config.run_all,
                                                     config.run_changed,
                                                     config.target_index)
      if targets != new_targets:
        # Note that this can happen, for example, if you rename a test target.
        click.echo('gn config was changed, trying to build again', err=True)
        targets = new_targets
        build_ok = test_executor.BuildTestTargets(out_dir, targets,
                                                  config.dry_run, config.quiet,
                                                  True)
    telemetry.RecordMainAttributes(targets, current_gtest_filter, used_cache,
                                   out_dir)

    if not build_ok:
      return 1

  return test_executor.RunTestTargets(out_dir, targets, current_gtest_filter,
                                      pref_mapping_filter, config.extras,
                                      config.dry_run,
                                      config.no_try_android_wrappers,
                                      config.no_fast_local_dev,
                                      config.no_single_variant)

if __name__ == '__main__':
  telemetry.telemetry.initialize('chromium.tools.autotest')

  try:
    sys.exit(main(prog_name='tools/autotest.py'))
  except (AutotestError, CommandError) as e:
    print(e, file=sys.stderr)
    sys.exit(1)

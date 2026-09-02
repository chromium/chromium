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
from utils.builders import display_utr_help, run_utr_tests

sys.path.append(str(const.SRC_DIR / 'build' / 'android'))
from pylib import constants

import logging
from colorama import init, Fore, Style


class AutotestLogFormatter(logging.Formatter):
  def format(self, record):
    msg = record.getMessage()
    color = getattr(record, 'color', None)
    prefix = ""

    if not color:
      if record.levelno == logging.WARNING:
        color = Fore.YELLOW
        prefix = "Warning: "
      elif record.levelno >= logging.ERROR:
        color = Fore.RED
        prefix = "Error: "

    if color:
      msg = f"{color}{prefix}{msg}{Style.RESET_ALL}"
    return msg


def configure_logging():
  init()
  handler = logging.StreamHandler(sys.stdout)
  handler.setFormatter(AutotestLogFormatter())
  level = logging.DEBUG if const.DEBUG else logging.INFO
  logging.basicConfig(level=level, handlers=[handler])


@click.command(
  cls=Formatter,
  help=__doc__,
  context_settings=dict(
    ignore_unknown_options=True,
    allow_interspersed_args=True,
    allow_extra_args=True,
    help_option_names=['-h', '--help'],
  ),
)
@autotest_options
@click.pass_context
@telemetry.tracer.start_as_current_span('chromium.tools.autotest.main')
def main(ctx, **kwargs) -> int:
  configure_logging()

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
  if kwargs.get('device'):
    extras.extend(['-d', kwargs['device']])
  kwargs['extras'] = extras

  config: AutotestConfig = AutotestConfig(**kwargs)

  if config.builder and not (
    config.run_changed
    or config.run_related
    or config.files
    or config.name
    or config.target
  ):
    display_utr_help()
    ctx.exit(0)

  if config.out_dir:
    constants.SetOutputDirectory(config.out_dir)
  constants.CheckOutputDirectory()
  out_dir = constants.GetOutDirectory()

  if not os.path.isdir(out_dir):
    raise click.UsageError(f'OUT_DIR "{out_dir}" does not exist.')

  target_cache: target_finder.TargetCache = target_finder.TargetCache(out_dir)

  if (
    not config.run_changed
    and not config.run_related
    and not config.files
    and not config.name
    and not config.target
  ):
    raise click.UsageError(
      'Specify a file to test or use --run-changed or --run-related'
    )

  direct_suites = []
  if config.suite:
    for f in config.files:
      direct_suites.append(f)
    config.files = tuple()
    config.run_changed = False
    if config.name:
      config.name = None

  # Cog is almost unusable with local search, so turn on remote_search.
  use_remote_search: bool = config.remote_search
  if not use_remote_search and const.SRC_DIR.parts[:3] == (
    '/',
    'google',
    'cog',
  ):
    logging.debug('Detected cog, turning on remote-search.')
    use_remote_search = True

  # Don't try to search if rg is not installed, and use the old behavior.
  if not use_remote_search and not shutil.which('rg'):
    if not config.quiet:
      logging.info(
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
      test_names, config.quiet, use_remote_search
    )
    if not current_gtest_filter:
      current_gtest_filter = found_filter
    files_to_test.extend(found_files)

  if config.run_changed:
    files_to_test.extend(file_finder.GetChangedTestFiles(config.git_ref))

  if config.run_related:
    files_to_test.extend(
      file_finder.GetRelatedTestFiles(config.git_ref, use_remote_search)
    )

  # Ensure duplicates are removed.
  if config.run_changed or config.run_related:
    files_to_test = list(set(files_to_test))

  filenames: list[str] = []
  for f in files_to_test:
    filenames.extend(
      file_finder.FindMatchingTestFiles(
        f,
        use_remote_search,
        config.path_index,
        config.run_all or config.run_changed or config.run_related,
      )
    )

  web_test_files = {f for f in filenames if file_finder.IsWebTestFile(f)}
  gn_files = [f for f in filenames if f not in web_test_files]

  targets = []
  used_cache = False
  if config.target:
    targets = [t.removeprefix('//') for t in config.target]
  elif filenames:
    targets, used_cache = target_finder.FindTestTargets(
      target_cache,
      out_dir,
      filenames,
      config.run_all,
      config.run_changed or config.run_related,
      config.target_index,
      config.files,
    )
  elif not direct_suites:
    command.ExitWithMessage('No associated test files found.')

  # Add any direct suites
  for suite in direct_suites:
    target_name = suite.removeprefix('//')
    if target_name not in targets:
      targets.append(target_name)

  if not current_gtest_filter and not config.suite:
    if gn_files:
      current_gtest_filter = filters.BuildTestFilter(gn_files, config.line)

  if not current_gtest_filter and not config.suite and gn_files:
    command.ExitWithMessage('Failed to derive a gtest filter')

  pref_mapping_filter: str | None = config.test_policy_to_pref_mappings_filter
  if not pref_mapping_filter:
    pref_mapping_filter = filters.BuildPrefMappingTestFilter(gn_files)

  assert targets

  build_ok: bool = True
  if not config.no_build:
    build_ok = test_executor.BuildTestTargets(
      out_dir, targets, config.dry_run, config.quiet, False
    )

    # If we used the target cache, it's possible we chose the wrong target
    # because a gn file was changed. The build step above will check for gn
    # modifications and update build.ninja. Use this opportunity the verify the
    # cache is still valid.
    if used_cache and not target_cache.IsStillValid():
      target_cache = target_finder.TargetCache(out_dir)
      new_targets, _ = target_finder.FindTestTargets(
        target_cache,
        out_dir,
        filenames,
        config.run_all,
        config.run_changed or config.run_related,
        config.target_index,
        config.files,
      )
      if targets != new_targets:
        # Note that this can happen, for example, if you rename a test target.
        logging.warning('gn config was changed, trying to build again')
        targets = new_targets
        build_ok = test_executor.BuildTestTargets(
          out_dir, targets, config.dry_run, config.quiet, True
        )

  telemetry.RecordMainAttributes(
    targets, current_gtest_filter or '*', used_cache, out_dir
  )

  if not build_ok:
    ctx.exit(1)

  if config.builder:
    ctx.exit(run_utr_tests(config, out_dir, targets))

  ctx.exit(
    test_executor.RunTestTargets(
      out_dir,
      targets,
      current_gtest_filter,
      pref_mapping_filter,
      config.extras,
      config.dry_run,
      config.no_try_android_wrappers,
      config.no_fast_local_dev,
      config.no_single_variant,
      is_suite=config.suite,
      gemini=config.gemini,
      web_test_files=web_test_files,
    )
  )


if __name__ == '__main__':
  telemetry.telemetry.initialize('chromium.tools.autotest')

  try:
    main(prog_name='tools/autotest.py')
  except (AutotestError, CommandError) as e:
    logging.error(e)
    sys.exit(1)

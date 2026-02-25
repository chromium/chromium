# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import click

from dataclasses import dataclass, fields


@dataclass
class AutotestConfig:
  out_dir: str | None
  remote_search: bool | None
  name: list[str] | None
  run_all: bool | None
  target_index: str | None
  path_index: str | None
  run_changed: bool | None
  line: int | None
  gtest_filter: str | None
  test_policy_to_pref_mappings_filter: str | None
  dry_run: bool | None
  quiet: bool | None
  no_try_android_wrappers: bool | None
  no_fast_local_dev: bool | None
  no_single_variant: bool | None
  no_build: bool | None
  files: tuple[str, ...]
  extras: list[str] | None = None  # To hold ctx.args


class Formatter(click.Command):

  def format_help(self, ctx: click.Context, formatter: click.HelpFormatter):
    """Overriding format_help to ensure the description is raw/unwrapped."""
    self.format_usage(ctx, formatter)

    if self.help:
      formatter.write(f"\n{self.help}\n")

    self.format_options(ctx, formatter)
    self.format_epilog(ctx, formatter)

  def format_usage(self, ctx: click.Context, formatter: click.HelpFormatter):

    prog_name: str = ctx.command_path

    pieces: list[str] = []
    for param in self.get_params(ctx):
      if isinstance(param, click.Option):

        opts: list[str] = param.opts + param.secondary_opts

        main_opt: str = max(opts, key=len) if opts else ""

        if param.is_flag:
          pieces.append(f"[{main_opt}]")
        else:
          metavar = param.metavar or param.name.upper()
          pieces.append(f"[{main_opt} {metavar}]")

    pieces.append("[FILE_NAME ...]")

    formatter.write_usage(prog_name,
                          " ".join(pieces),
                          prefix='Usage: vpython3 ')

  def format_options(self, ctx: click.Context, formatter: click.HelpFormatter):

    with formatter.section('Positional arguments'):
      formatter.write_dl([('FILE_NAME',
                           'test suite file (eg. FooTest.java) or test name')])

    opts: list[tuple[str, str]] = []
    for param in self.get_params(ctx):

      if isinstance(param, click.Option):
        record: tuple[str, str] = param.get_help_record(ctx)
        if record:
          opts.append(record)

    with formatter.section('Options'):
      formatter.write_dl(opts)


def autotest_options(f):
  """Decorator to group all autotest CLI options."""
  options = [
      click.option('--out-dir',
                   '--out_dir',
                   '--output-directory',
                   '--output_directory',
                   '-C',
                   metavar='OUT_DIR',
                   help='output directory of the build'),
      click.option('--remote-search',
                   '--remote_search',
                   '-r',
                   is_flag=True,
                   help='Search for tests using a remote service'),
      click.option('--name',
                   multiple=True,
                   help='Search for the test by name, and apply test filter'),
      click.option(
          '--run-all',
          '--run_all',
          is_flag=True,
          help='Run all tests for the file or directory, instead of just one'),
      click.option(
          '--target-index',
          '--target_index',
          type=int,
          help='When the target is ambiguous, choose the one with this index.'),
      click.option(
          '--path-index',
          '--path_index',
          type=int,
          help='When the test path is ambiguous, choose the one with this index.'
      ),
      click.option(
          '--run-changed',
          '--run_changed',
          is_flag=True,
          help='Run tests files modified since this branch diverged from main.'
      ),
      click.option('--line',
                   type=int,
                   help='run only the test on this line number. c++ only.'),
      click.option('--gtest-filter',
                   '--gtest_filter',
                   '-f',
                   metavar='FILTER',
                   help='test filter'),
      click.option('--test-policy-to-pref-mappings-filter',
                   '--test_policy_to_pref_mappings_filter',
                   metavar='FILTER',
                   help='policy pref mappings test filter'),
      click.option(
          '--dry-run',
          '--dry_run',
          '-n',
          is_flag=True,
          help='Print ninja and test run commands without executing them.'),
      click.option(
          '--quiet',
          '-q',
          is_flag=True,
          help='Do not print while building, only print if build fails.'),
      click.option(
          '--no-try-android-wrappers',
          '--no_try_android_wrappers',
          is_flag=True,
          help='Do not try to use Android test wrappers to run tests.'),
      click.option('--no-fast-local-dev',
                   '--no_fast_local_dev',
                   is_flag=True,
                   help='Do not add --fast-local-dev for Android tests.'),
      click.option('--no-single-variant',
                   '--no_single_variant',
                   is_flag=True,
                   help='Do not add --single-variant for Android tests.'),
      click.option('--no-build',
                   '--no_build',
                   is_flag=True,
                   help='Do not build before running tests.'),
  ]
  # Apply in reverse so the first item in the list appears first in --help
  for option in reversed(options):
    f = option(f)
  return f

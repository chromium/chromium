# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shlex
import subprocess
import sys
import logging
from colorama import Fore

import utils.command_util as command
import utils.constants as const
import utils.telemetry as telemetry

sys.path.append(str(const.SRC_DIR / 'build'))
import gn_helpers


def _log_build(msg: str):
  logging.info(msg, extra={"color": Fore.CYAN})


def _log_run(msg: str):
  logging.info(msg, extra={"color": Fore.GREEN})


def _log_failure(msg: str):
  logging.info(msg, extra={"color": Fore.RED})


@telemetry.tracer.start_as_current_span('chromium.tools.autotest.build')
def BuildTestTargets(
  out_dir: str, targets: list[str], dry_run: bool, quiet: bool, is_retry: bool
) -> bool:
  """Builds the specified targets with ninja"""
  cmd: list[str] = gn_helpers.CreateBuildCommand(out_dir) + targets
  _log_build(f"\n>>> Building {len(targets)} target(s): {shlex.join(cmd)}\n")
  if dry_run:
    return True

  should_capture = quiet or not sys.stdout.isatty()
  completed_process = subprocess.run(
    cmd, capture_output=should_capture, encoding='utf-8'
  )

  is_successful = completed_process.returncode == 0
  telemetry.RecordBuildAttributes(is_retry, is_successful)

  if is_successful:
    _log_build("\n<<< Build completed successfully\n")
  else:
    if should_capture:
      before, _, after = completed_process.stdout.partition('stderr:')
      if not after:
        before, _, after = completed_process.stdout.partition('stdout:')
      output = after or before
      print(output)
      if completed_process.stderr:
        print(completed_process.stderr)
    _log_failure("\n<<< Build failed\n")

  return is_successful


def _CreateWebTestCommand(
  out_dir: str,
  web_test_files: list[str] | set[str] | None,
  extra_args: list[str],
) -> list[str]:
  run_web_tests_path = os.path.join(
    const.SRC_DIR, 'third_party', 'blink', 'tools', 'run_web_tests.py'
  )
  cmd = [
    'vpython3',
    run_web_tests_path,
    f'--build-directory={out_dir}',
    '--no-build',
  ]
  if web_test_files:
    cmd.extend(sorted(web_test_files))
  cmd.extend(extra_args)
  return cmd


def _CreateGtestCommand(
  out_dir: str,
  target_binary: str,
  gtest_filter: str | None,
  pref_mapping_filter: str | None,
  extra_args: list[str],
  no_try_android_wrappers: bool,
  no_fast_local_dev: bool,
  no_single_variant: bool,
) -> list[str]:
  path: str = os.path.join(out_dir, 'bin', f'run_{target_binary}')
  target_extra_args = list(extra_args)
  if no_try_android_wrappers or not os.path.isfile(path):
    path = os.path.join(out_dir, target_binary)
  else:
    if not no_fast_local_dev:
      target_extra_args.append('--fast-local-dev')
    if not no_single_variant:
      target_extra_args.append('--single-variant')

  cmd: list[str] = [path]
  if gtest_filter:
    cmd.append(f'--gtest_filter={gtest_filter}')
  if pref_mapping_filter:
    cmd.append(f'--test_policy_to_pref_mappings_filter={pref_mapping_filter}')

  cmd.extend(target_extra_args)
  return cmd


def RunTestTargets(
  out_dir: str,
  targets: list[str],
  gtest_filter: str,
  pref_mapping_filter: str | None,
  extra_args: list[str],
  dry_run: bool,
  no_try_android_wrappers: bool,
  no_fast_local_dev: bool,
  no_single_variant: bool,
  is_suite: bool = False,
  web_test_files: list[str] | set[str] | None = None,
) -> int:
  total_passed = total_failed = 0
  failed_test_names = []
  any_failed = False

  for target in targets:
    target_binary: str = target.split(':')[-1]

    if target_binary == 'blink_tests':
      test_type = const.TestType.WEB
      cmd = _CreateWebTestCommand(out_dir, web_test_files, extra_args)
    else:
      test_type = const.TestType.GTEST
      cmd = _CreateGtestCommand(
        out_dir,
        target_binary,
        gtest_filter,
        pref_mapping_filter,
        extra_args,
        no_try_android_wrappers,
        no_fast_local_dev,
        no_single_variant,
      )

    command_str = shlex.join(cmd)
    _log_run(f"\n>>> Running {target_binary}")
    logging.info(f"Command: {command_str}\n")
    if dry_run:
      continue

    return_code, summary = command.RunTestCommandWithSummary(
      cmd, test_type=test_type
    )

    if return_code != 0:
      _log_failure(
        f"\n<<< {target_binary} failed with exit code {return_code}\n"
      )

    if not is_suite and len(targets) <= 1:
      if return_code != 0:
        return return_code
      continue

    if summary.parse_error:
      logging.warning(
        f"Could not parse test summary JSON for {target_binary}. {summary.parse_error}"
      )
    else:
      total_passed += len(summary.passed_tests)
      total_failed += len(summary.failed_tests)
      for test_name, _ in summary.failed_tests:
        failed_test_names.append(f"{target_binary}: {test_name}")

    if return_code != 0:
      any_failed = True

  if dry_run or (not is_suite and len(targets) <= 1):
    return 0

  logging.info('=' * 40)
  logging.info('EXECUTION SUMMARY')
  logging.info('=' * 40)
  logging.info(f'Total Tests Passed/Skipped: {total_passed}')
  logging.info(f'Total Tests Failed:         {total_failed}')
  logging.info('=' * 40)

  if failed_test_names:
    logging.info('FAILED TESTS:')
    for test in failed_test_names:
      logging.info(f'  - {test}')

  return 1 if any_failed else 0

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shlex
import subprocess
import sys

import utils.command_util as command
import utils.constants as const
import utils.telemetry as telemetry

sys.path.append(str(const.SRC_DIR / 'build'))
import gn_helpers


@telemetry.tracer.start_as_current_span('chromium.tools.autotest.build')
def BuildTestTargets(out_dir: str, targets: list[str], dry_run: bool,
                     quiet: bool, is_retry: bool) -> bool:
  """Builds the specified targets with ninja"""
  cmd: list[str] = gn_helpers.CreateBuildCommand(out_dir) + targets
  print('Building: ' + shlex.join(cmd))
  if (dry_run):
    return True

  completed_process: subprocess.CompletedProcess[str] = subprocess.run(
      cmd, capture_output=quiet, encoding='utf-8')

  telemetry.RecordBuildAttributes(is_retry, completed_process.returncode == 0)

  if completed_process.returncode != 0:
    if quiet:
      before, _, after = completed_process.stdout.partition('stderr:')
      if not after:
        before, _, after = completed_process.stdout.partition('stdout:')
      if after:
        print(after)
      else:
        print(before)
    return False
  return True


def RunTestTargets(out_dir: str, targets: list[str], gtest_filter: str,
                   pref_mapping_filter: str | None, extra_args: list[str],
                   dry_run: bool, no_try_android_wrappers: bool,
                   no_fast_local_dev: bool, no_single_variant: bool) -> int:

  for target in targets:
    target_binary: str = target.split(':')[1]

    # Look for the Android wrapper script first.
    path: str = os.path.join(out_dir, 'bin', f'run_{target_binary}')
    if no_try_android_wrappers or not os.path.isfile(path):
      # If the wrapper is not found or disabled use the Desktop target
      # which is an executable.
      path = os.path.join(out_dir, target_binary)
    else:
      if not no_fast_local_dev:
        # Usually want this flag when developing locally.
        extra_args = extra_args + ['--fast-local-dev']
      if not no_single_variant:
        extra_args = extra_args + ['--single-variant']

    cmd: list[str] = [path, f'--gtest_filter={gtest_filter}']
    if pref_mapping_filter:
      cmd.append(f'--test_policy_to_pref_mappings_filter={pref_mapping_filter}')
    cmd.extend(extra_args)

    print('Running test: ' + shlex.join(cmd))
    if not dry_run:
      return_code = command.StreamCommandOrExit(cmd)
      if return_code != 0:
        return return_code
  return 0

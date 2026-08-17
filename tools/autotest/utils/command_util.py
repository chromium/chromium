# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import locale
import os
import shlex
import subprocess
import sys
import tempfile
import logging

from . import constants as const
from . import telemetry
import utils

from .command_error import CommandError, AutotestError
from .test_summary import ParseTests, ParseWebTestResults, TestSummary


def ExitWithMessage(*args: list[str]):
  raise AutotestError(' '.join(map(str, args)))


class GtestConfig:
  @property
  def user_path_flags(self) -> list[str]:
    return ['--test-launcher-summary-output', '--json-results-file']

  def GetOutputFlags(self, path: str) -> list[str]:
    return [
      f'--test-launcher-summary-output={path}',
      f'--json-results-file={path}',
    ]

  def ParseResults(self, path: str) -> TestSummary:
    return ParseTests(path)


class WebTestConfig:
  @property
  def user_path_flags(self) -> list[str]:
    return ['--json-test-results']

  def GetOutputFlags(self, path: str) -> list[str]:
    return [f'--json-test-results={path}']

  def ParseResults(self, path: str) -> TestSummary:
    return ParseWebTestResults(path)


@telemetry.tracer.start_as_current_span('chromium.tools.autotest.run_target')
def RunTestCommandWithSummary(
  cmd: list[str],
  test_type: const.TestType = const.TestType.GTEST,
  **kwargs: int,
) -> tuple[int, TestSummary]:
  runner_config = (
    GtestConfig() if test_type == const.TestType.GTEST else WebTestConfig()
  )
  user_provided_path: str = None

  for flag in runner_config.user_path_flags:
    for i, arg in enumerate(cmd):
      if arg.startswith(f'{flag}='):
        user_provided_path = arg.split('=', 1)[1]
        break
      elif arg == flag and i + 1 < len(cmd):
        user_provided_path = cmd[i + 1]
        break
    if user_provided_path:
      break

  def _run_and_parse_tests(cmd: list[str], path: str):
    result: subprocess.CompletedProcess[str] = subprocess.run(
      cmd, check=False, **kwargs
    )
    test_summary: TestSummary = runner_config.ParseResults(path)
    is_successful: bool = result.returncode == 0

    telemetry.RecordRunAttributes(cmd, is_successful, test_summary)

    return result.returncode, test_summary

  if user_provided_path:
    return _run_and_parse_tests(cmd, user_provided_path)

  else:
    with tempfile.NamedTemporaryFile(mode='w+', suffix='.json') as tmp:
      cmd.extend(runner_config.GetOutputFlags(tmp.name))
      return _run_and_parse_tests(cmd, tmp.name)


def RunCommand(cmd: list[str], **kwargs: int) -> str:
  # `shlex` does not support `pathlib.Path`, which `RunCommand` is sometimes
  # called with. We explicitly convert the args into a `str` to be safe.
  logging.debug(f"Run command: {shlex.join([str(c) for c in cmd])}")

  try:
    # Set an encoding to convert the binary output to a string.
    return subprocess.check_output(
      cmd, **kwargs, encoding=locale.getpreferredencoding()
    )
  except subprocess.CalledProcessError as e:
    raise CommandError(e.cmd, e.returncode, e.output) from None


def _ChooseByIndex(msg: str, options: list[str]) -> str:
  while True:
    try:
      user_input: str = input(msg)
    except EOFError:
      # Non-interactive, or user enter Ctrl-D.
      sys.exit(1)
    try:
      return options[int(user_input)]
    except (ValueError, IndexError):
      msg = 'Invalid index. Try again: '


def HaveUserPickFile(paths: list[str]) -> str:
  paths = sorted(paths, key=lambda p: (len(p), p))[:20]
  path_list: str = '\n'.join(f'{i}. {t}' for i, t in enumerate(paths))

  logging.info(f"""\
Found multiple paths with that name.
Hint: Avoid this in subsequent runs using --target=$TARGET_NAME, or --run-all

{path_list}
""")
  msg = 'Pick the path that you want by its index: '
  return _ChooseByIndex(msg, paths)


def HaveUserPickTarget(orig_paths: list[str], targets: list[str]) -> str:
  targets = targets[:20]
  target_list: str = '\n'.join(f'{i}. {t}' for i, t in enumerate(targets))

  fail_fast = False
  hint = """
Hint: To avoid this in subsequent runs:
 * Provide full paths (not just file names)
 * Add --run-all (to run all of them)
 * Add --target-index=$INDEX (to pick by index)
 * Add --target=foo_tests (to pick by name)
"""
  if orig_paths and not all(os.path.sep in p for p in orig_paths):
    if utils.IsLlm():
      # Get agents to use full paths to resolve multiple targets.
      fail_fast = True
      hint = 'Try again with full paths (not just base names).\n'
    else:
      hint += ' * Use full paths (not just base names)\n'

  logging.info(f"""\
Path(s) belong to multiple test targets.

{target_list}

{hint}""")
  if fail_fast:
    sys.exit(1)
  msg = 'Pick a target by its index: '
  return _ChooseByIndex(msg, targets)

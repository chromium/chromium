# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import locale
import shlex
import subprocess
import sys

from . import constants as const

from . import telemetry

from .command_error import CommandError, AutotestError


def ExitWithMessage(*args: list[str]):
  raise AutotestError(' '.join(map(str, args)))


@telemetry.tracer.start_as_current_span('chromium.tools.autotest.run_target')
def StreamCommandOrExit(cmd: list[str], **kwargs: int) -> int:
  result: subprocess.CompletedProcess[str] = subprocess.run(cmd,
                                                            check=False,
                                                            **kwargs)
  is_successful: bool = result.returncode == 0
  telemetry.RecordRunAttributes(cmd, is_successful)
  return result.returncode


def RunCommand(cmd: list[str], **kwargs: int) -> str:
  if const.DEBUG:
    # `shlex` does not support `pathlib.Path`, which `RunCommand` is sometimes
    # called with. We explicitly convert the args into a `str` to be safe.
    print(f"Run command: {shlex.join([str(c) for c in cmd])}")

  try:
    # Set an encoding to convert the binary output to a string.
    return subprocess.check_output(cmd,
                                   **kwargs,
                                   encoding=locale.getpreferredencoding())
  except subprocess.CalledProcessError as e:
    raise CommandError(e.cmd, e.returncode, e.output) from None


def _ChooseByIndex(msg: str, options: list[str]) -> str:
  while True:
    user_input: str = input(msg)
    try:
      return options[int(user_input)]
    except (ValueError, IndexError):
      msg = 'Invalid index. Try again: '


def HaveUserPickFile(paths: list[str]) -> str:
  paths = sorted(paths, key=lambda p: (len(p), p))[:20]
  path_list: str = '\n'.join(f'{i}. {t}' for i, t in enumerate(paths))

  msg: str = f"""\
Found multiple paths with that name.
Hint: Avoid this in subsequent runs using --path-index=$INDEX, or --run-all.

{path_list}

Pick the path that you want by its index: """
  return _ChooseByIndex(msg, paths)


def HaveUserPickTarget(paths: list[str], targets: list[str]) -> str:
  targets = targets[:20]
  target_list: str = '\n'.join(f'{i}. {t}' for i, t in enumerate(targets))

  msg: str = f"""\
Path(s) belong to multiple test targets.
Hint: Avoid this in subsequent runs using --target-index=$INDEX, or --run-all.

{target_list}

Pick a target by its index: """
  return _ChooseByIndex(msg, targets)

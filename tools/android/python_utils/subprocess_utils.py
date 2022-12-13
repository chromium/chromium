# Lint as: python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper functions for running commands as subprocesses."""

import logging
import pathlib
import shutil
import sys
import subprocess

from typing import Optional, Sequence, Union

_PYTHON_UTILS_PATH = pathlib.Path(__file__).resolve().parents[0]
if str(_PYTHON_UTILS_PATH) not in sys.path:
    sys.path.append(str(_PYTHON_UTILS_PATH))
import git_metadata_utils


def resolve_ninja() -> str:
    # Prefer the version on PATH, but fallback to known version if PATH doesn't
    # have one (e.g. on bots).
    if shutil.which('ninja') is None:
        return str(git_metadata_utils.get_chromium_src_path() /
                   'third_party/ninja/ninja')
    return 'ninja'


def resolve_autoninja():
    # Prefer the version on PATH, but fallback to known version if PATH doesn't
    # have one (e.g. on bots).
    if shutil.which('autoninja') is None:
        return str(git_metadata_utils.get_chromium_src_path() /
                   'third_party/depot_tools/autoninja')
    return 'autoninja'


def run_command(
        command: Sequence[str],
        *,  # Ensures that the rest of the args are passed explicitly.
        cwd: Optional[Union[str, pathlib.Path]] = None,
        cmd_input: Optional[str] = None,
        exitcode_only: bool = False) -> str:
    """Runs a command and returns the output as a string.

  For example, run_command(['echo', ' Hello, world!\n']) returns the string
  'Hello, world!' and run_command(['pwd'], cwd='/usr/local/bin') returns the
  string '/usr/local/bin'.

  Args:
    command:
      A sequence of strings representing the command to run, starting with the
      executable name followed by the arguments.
    cwd:
      The working directory in which to run the command; if not specified,
      defaults to the current working directory.
    cmd_input:
      Input text that should be automatically passed to the process's stdin.
    exitcode_only:
      Avoid re-raising any errors or logging any output for errors. Useful for
      commands that are expected to return a non-zero exit status.

  Returns:
    If |exitcode_only| is True, then only the exitcode is returned.
    Otherwise, output (stdout) of the command as a string. Leading and trailing
    whitespace are stripped from the output.

  Raises
    CalledProcessError:
      The command returned a non-zero exit code indicating failure; the error
      code is printed along with the error message (stderr) and output (stdout),
      if any.
    FileNotFoundError: The executable specified in the command was not found.
  """
    try:
        run_result: subprocess.CompletedProcess = subprocess.run(
            command,
            capture_output=True,
            text=True,
            check=not exitcode_only,
            cwd=cwd,
            input=cmd_input)
    except subprocess.CalledProcessError as e:
        command_str = ' '.join(command)
        error_msg = f'Command "{command_str}" failed with code {e.returncode}.'
        if e.stderr:
            error_msg += f'\nSTDERR: {e.stderr}'
        if e.stdout:
            error_msg += f'\nSTDOUT: {e.stdout}'

        logging.error(error_msg)
        raise

    if exitcode_only:
        return run_result.returncode
    return str(run_result.stdout).strip()

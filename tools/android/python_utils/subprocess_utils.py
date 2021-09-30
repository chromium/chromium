# Lint as: python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper functions for running commands as subprocesses."""

import logging
import subprocess

from typing import Optional, Sequence


def run_command(command: Sequence[str], cwd: Optional[str] = None) -> str:
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

  Returns:
    The output (stdout) of the command as a string. Leading and trailing
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
            command, capture_output=True, text=True, check=True, cwd=cwd)
    except subprocess.CalledProcessError as e:
        command_str = ' '.join(command)
        error_msg = f'Command "{command_str}" failed with code {e.returncode}.'
        if e.stderr:
            error_msg += f'\nSTDERR: {e.stderr}'
        if e.stdout:
            error_msg += f'\nSTDOUT: {e.stdout}'
        logging.error(error_msg)

        raise

    return str(run_result.stdout).strip()

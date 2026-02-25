# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from typing import List, Tuple
import sys
import time
import subprocess

import setup_modules

from chromium_src.tools.metrics.python_support.tests_helpers import TestableScript


def check_scripts(commands_to_check: List[TestableScript],
                  cwd: str,
                  display_progressbar: bool = True) -> List[Tuple[str, int]]:
  """Checks if the provided python scripts finish successfully.

  Args:
    commands_to_check - a list of TestableScript to be executed.
    cwd - working directory to run the script in.
    display_progressbar - if True, the progressbar will be printed to stdout.

  Returns:
    A list of name of commands that failed with their status code
  """
  running_processes = []
  failed_commands = []
  total_commands = len(commands_to_check)
  completed_count = 0
  last_completed_count = -1

  for testable_script in commands_to_check:
    cmd_debug_str = " ".join(testable_script.cmd)
    print(f"Running: {cmd_debug_str}")
    proc = subprocess.Popen(testable_script.cmd,
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL,
                            cwd=cwd)
    running_processes.append((testable_script, proc))

  while running_processes:
    # Iterating backwards to safely remove items form list
    for i in range(len(running_processes) - 1, -1, -1):
      testable_script, proc = running_processes[i]

      exit_code = proc.poll()

      if exit_code is not None:
        if exit_code != 0:
          failed_commands.append((testable_script.identifiable_name, exit_code))

        completed_count += 1
        del running_processes[i]

    if display_progressbar and completed_count != last_completed_count:
      _print_progress_bar(completed_count, total_commands)
    last_completed_count = completed_count

    if running_processes:
      time.sleep(0.5)

  return failed_commands


def _print_progress_bar(iteration, total, length=40):
  """Helper to print a progress bar to stdout using standard libraries."""
  if total == 0:
    return

  percent = ("{0:.1f}").format(100 * (iteration / float(total)))
  filled_length = int(length * iteration // total)
  bar = '█' * filled_length + '-' * (length - filled_length)

  sys.stdout.write(
      f'\rWaiting for the scripts to finish: |{bar}| {percent}% Complete')
  sys.stdout.flush()

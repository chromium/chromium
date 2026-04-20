# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utils for invoking UTR (Universal Test Runner) from autotest."""

import argparse
import os
import subprocess
import sys
from dataclasses import dataclass

from utils.command_error import AutotestError


def display_utr_help():
  """Displays UTR help with overridden usage."""
  utr_path = os.path.join(os.path.dirname(__file__), '..', '..', 'utr')
  try:
    result = subprocess.run([sys.executable, utr_path, '--help'],
                            capture_output=True,
                            text=True,
                            check=True)
    output = result.stdout
    # Override usage
    output = output.replace('usage: utr', 'usage: autotest.py --builder')

    # Remove the positional arguments section, as autotest hardcodes 'test' mode
    pos_start = output.find("positional arguments:")
    if pos_start != -1:
      next_section = output.find("\noptions:\n", pos_start)
      if next_section == -1:
        next_section = output.find("\noptional arguments:\n", pos_start)
      if next_section != -1:
        note = (
            "Note: The commands above will invoke UTR directly.\n\n"
            "When invoked via autotest.py --builder, UTR is always executed "
            "in 'test'\nmode by default. The corresponding autotest command is:"
        )

        example = (
            "\n\nvpython3 tools/autotest.py --builder -B $BUCKET -b $BUILDER "
            "-t $TEST --\n    --gtest_filter=Test.Case\n")

        output = output[:pos_start] + note + example + output[next_section:]

    print(output)
  except subprocess.CalledProcessError as e:

    print(f"Error calling UTR help: {e}", file=sys.stderr)
    print(e.stderr, file=sys.stderr)


def run_remote_test(config, out_dir, targets):
  """Invokes UTR in test mode.

  Args:
    config: AutotestConfig object containing parsed arguments.
    out_dir: The build directory.
    targets: List of test targets.

  Returns:
    Exit code from UTR.
  """
  utr_path = os.path.join(os.path.dirname(__file__), '..', '..', 'utr')

  command = [sys.executable, utr_path]

  # Pass all extra arguments directly to UTR
  if config.extras:
    command.extend(config.extras)

  command.extend(['-o', out_dir])
  for target in targets:
    suite_name = target.split(':')[-1] if ':' in target else target
    command.extend(['-t', suite_name])

  # Hardcode 'test' since autotest will take care of build
  command.append('test')

  print(f"Invoking UTR: {' '.join(command)}")

  if config.dry_run:
    print("Dry run: skipping execution.")
    return 0

  try:
    result = subprocess.run(command, check=False)
    return result.returncode
  except Exception as e:
    raise AutotestError(f"Failed to invoke UTR: {e}")

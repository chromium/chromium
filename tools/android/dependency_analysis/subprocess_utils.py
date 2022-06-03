#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utils to run subprocesses."""

import subprocess
import sys

from typing import List


def run_command(command: List[str]) -> str:
    """Runs a command and returns the output.

    Raises an exception and prints the command output if the command fails."""
    try:
        run_result = subprocess.run(command,
                                    capture_output=True,
                                    text=True,
                                    check=True)
    except subprocess.CalledProcessError as e:
        print(f'{command} failed with code {e.returncode}.', file=sys.stderr)
        print(f'\nSTDERR:\n{e.stderr}', file=sys.stderr)
        print(f'\nSTDOUT:\n{e.stdout}', file=sys.stderr)
        raise
    return run_result.stdout

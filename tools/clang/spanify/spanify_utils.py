# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared utilities for spanification script configurations."""

import functools
import os
import pathlib
import shutil
import subprocess
import sys


@functools.cache
def scratch_dir():
    """Returns the path to the scratch directory as a pathlib.Path."""
    if env_dir := os.environ.get("SPANIFY_SCRATCH_DIR"):
        path = pathlib.Path(env_dir)
    else:
        path = pathlib.Path.cwd() / "spanify_scratch"
        print(f"No SPANIFY_SCRATCH_DIR set falling back to {path}")

    path.mkdir(parents=True, exist_ok=True)
    return path


def clear_scratch_dir():
    """Clears the contents of the scratch directory without deleting the directory itself."""
    for item in scratch_dir().iterdir():
        if item.is_dir():
            shutil.rmtree(item, ignore_errors=True)
        else:
            item.unlink(missing_ok=True)


def apply_collected_edits(replacements, platform="linux"):
    """
    Numerical offset descending order sorting procedure.
    Outputs processed data piped directly to tool: apply_edits.py.
    """
    # The output contains lines with the format:
    # r:::{0}:::{1}:::{2}:::{3}
    # We must sort by {1} numerically in descending order to avoid conflict when
    # applying replacements.
    # {0} is the file path
    # {1} is the file offset
    # {2} is the replacement length
    # {3} is the replacement text
    sorted_replacements = []
    for r in replacements:
        r = r.strip()
        if not r:
            continue
        parts = r.split(':::')
        try:
            offset = int(parts[2])
            sorted_replacements.append((offset, r))
        except (ValueError, IndexError):
            print("Error: Dropping invalid replacement", file=sys.stderr)
            continue

    sorted_replacements.sort(key=lambda x: x[0], reverse=True)

    input_text = '\n'.join([x[1] for x in sorted_replacements]) + '\n'

    apply_edits_script = "tools/clang/scripts/apply_edits.py"

    subprocess.run(
        [sys.executable,
         str(apply_edits_script), "-p", f"./out/{platform}"],
        input=input_text,
        text=True,
        check=True)

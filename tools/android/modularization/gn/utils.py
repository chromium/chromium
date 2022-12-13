# Lint as: python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import pathlib
import sys

_TOOLS_ANDROID_PATH = pathlib.Path(__file__).resolve().parents[2]
if str(_TOOLS_ANDROID_PATH) not in sys.path:
    sys.path.append(str(_TOOLS_ANDROID_PATH))
from python_utils import subprocess_utils

# These paths should be relative to repository root.
_BAD_FILES = [
    # Malformed BUILD.gn file, remove this entry once it is fixed.
    "third_party/swiftshader/tests/VulkanUnitTests/BUILD.gn",
]


def is_bad_gn_file(filepath: str, root: str) -> bool:
    relpath = os.path.relpath(filepath, root)
    for bad_filepath in _BAD_FILES:
        if relpath == bad_filepath:
            logging.warning(f'Skipping {relpath}: found in _BAD_FILES list.')
            return True
    if not os.access(filepath, os.R_OK | os.W_OK):
        logging.warning(f'Skipping {relpath}: Cannot read and write to it.')
        return True
    return False


def is_git_ignored(root: pathlib.Path, filepath: str) -> bool:
    # The command git check-ignore exits with 0 if the path is ignored, 1 if it
    # is not ignored.
    exit_code = subprocess_utils.run_command(
        ['git', 'check-ignore', '-q', filepath], cwd=root, exitcode_only=True)
    return exit_code == 0

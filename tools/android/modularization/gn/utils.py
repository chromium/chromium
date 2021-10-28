# Lint as: python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import sys

_TOOLS_ANDROID_PATH = pathlib.Path(__file__).parents[2].resolve()
if str(_TOOLS_ANDROID_PATH) not in sys.path:
    sys.path.append(str(_TOOLS_ANDROID_PATH))
from python_utils import subprocess_utils

_BAD_FILES = [
    # Malformed BUILD.gn file, remove this entry once it is fixed.
    "third_party/swiftshader/tests/VulkanUnitTests/BUILD.gn",
]


def is_bad_gn_file(filepath: str) -> bool:
    for bad_filepath in _BAD_FILES:
        if bad_filepath.endswith(filepath) or filepath.endswith(bad_filepath):
            return True
    return False


def is_git_ignored(root: pathlib.Path, filepath: str) -> bool:
    # The command git check-ignore exits with 0 if the path is ignored, 1 if it
    # is not ignored.
    exit_code = subprocess_utils.run_command(
        ['git', 'check-ignore', '-q', filepath], cwd=root, exitcode_only=True)
    return exit_code == 0

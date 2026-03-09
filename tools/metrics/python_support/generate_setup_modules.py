# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This tool generates setup_modules.py in all subdirectories of tools/metrics
# except the ones on _EXCLUDE_LIST.
# See more details about this system in comments within generated files or
# in _TARGET_FILE_TEMPLATE below.
import os
from pathlib import Path
from typing import List

_TOOLS_METRICS_RELATIVE_PATH = 'tools/metrics'

# List of subdirectories to ignore completely when generating the file.
_EXCLUDE_LIST: List[str] = [
    'histograms/metadata',
    'histograms/test_data',
    '.mypy_cache',
    'private_metrics/tests',
]

_TARGET_FILE_NAME = 'setup_modules.py'

_TARGET_FILE_TEMPLATE = """# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# AUTO GENERATED, DO NOT EDIT MANUALLY.
# SEE: tools/metrics/generate_setup_modules.py
#
# This is a shim allowing universal setup of tools/metrics scripts.
# It calls setup_modules_lib.setup_modules() to create a common
# environment whenever imported.
#
# Each subdirectory in tools/metrics includes a setup_modules.py
# file which should be imported at the top of each script, like so:
#
# ```
# import setup_modules
#
# # Example import afterwards: Importing src/tools/metrics/common/models.py
# import chrome_src.tools.metrics.common.models as models
# ```

from pathlib import Path

import os
import sys

_CHROMIUM_SRC_RELATIVE_PATH = '{chromium_src_relative_path}'

base_path = Path(os.path.dirname(os.path.abspath(__file__)))

# Add src/tools/metrics to path temporarily to import the setup_modules_lib.
chromium_src_path = base_path.joinpath(_CHROMIUM_SRC_RELATIVE_PATH).resolve()
setup_modules_path = chromium_src_path.joinpath('tools', 'metrics',
                                                'python_support').resolve()

sys.path.append(str(setup_modules_path))
import setup_modules_lib

# Restore the path to an extent that it's possible, so
# that it doesn't interfere with actual modules setup.
sys.path.remove(str(setup_modules_path))

# Actually set up the modules using setup_modules_lib.
setup_modules_lib.setup_modules(str(chromium_src_path))
"""


def _is_excluded(path: str) -> bool:
  for excluded_entry in _EXCLUDE_LIST:
    if path.startswith(excluded_entry):
      return True
  return False


def _generate_helpers(tools_metrics_path_str: str):
  tools_metrics_path = Path(tools_metrics_path_str).resolve()
  chromium_src_path = Path(
      tools_metrics_path_str.removesuffix(
          _TOOLS_METRICS_RELATIVE_PATH)).resolve()

  for dirpath, _, _ in os.walk(tools_metrics_path):
    current_dir = Path(dirpath).resolve()

    # Exclude based on relative path
    relative_path = current_dir.relative_to(tools_metrics_path)
    if _is_excluded(str(relative_path)):
      continue

    chromium_src_relative_path = Path(
        os.path.relpath(chromium_src_path, current_dir))
    file_content = _TARGET_FILE_TEMPLATE.format(
        chromium_src_relative_path=str(chromium_src_relative_path))
    file_path = current_dir.joinpath(_TARGET_FILE_NAME)

    with open(file_path, "w", encoding="utf-8") as f:
      f.write(file_content)
    print(f"Generated: {file_path}"
          f" (chromium relative path: {str(chromium_src_relative_path)})")


if __name__ == "__main__":
  _generate_helpers(tools_metrics_path_str=str(Path(__file__).parent.parent))

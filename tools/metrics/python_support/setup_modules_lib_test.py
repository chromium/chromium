# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Validates that imports are properly evaluated within tools/metrics
# python scripts.

import unittest
import os
import importlib.util
from pathlib import Path
from typing import List, Tuple

from parameterized import parameterized  # type: ignore

_TARGET_DIR = os.path.dirname(os.path.abspath(__file__))
_IMPORT_BASE = os.path.abspath(os.path.join(_TARGET_DIR, '..', '..'))

# Files that part of the setup_modules setup that will be ignored
_IGNORED_FILES = ['setup_modules.py', os.path.basename(__file__)]


def _get_python_files() -> List[Tuple[str, str]]:
  """Gets all the python (.py) files within tools/metrics.

  It ignores files with name listed in _IGNORED_FILES.

  Returns: List of files found
  """
  target_path = Path(_TARGET_DIR).resolve()
  files_list = []

  for root, _, files in os.walk(target_path):
    for file in files:
      if file.endswith('.py') and file not in _IGNORED_FILES:
        full_path = os.path.join(root, file)

        # Create a readable name for the test output
        rel_path = os.path.relpath(full_path, target_path)
        safe_name = rel_path.replace(os.sep, '_').replace('.', '_')

        files_list.append((safe_name, full_path))

  return files_list


class TestImports(unittest.TestCase):

  @parameterized.expand(_get_python_files)
  def test_compiles(self, _, file_path):
    file_path = Path(file_path)

    try:
      with open(file_path, 'r', encoding='utf-8') as f:
        source = f.read()
      compile(source, file_path, 'exec')
    except SyntaxError as e:
      self.fail(f'Syntax Error: {e}')
    except Exception as e:
      self.fail(f'File Read Error: {e}')

  @parameterized.expand(_get_python_files)
  def test_imports(self, _, file_path):
    file_path = Path(file_path)
    module_name = file_path.stem

    try:
      spec = importlib.util.spec_from_file_location(module_name, file_path)
      if spec and spec.loader:
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
    except (ImportError, ModuleNotFoundError) as e:
      self.fail(f'Import Error: {e}')


if __name__ == '__main__':
  unittest.main()

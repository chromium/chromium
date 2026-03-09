# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import types

from pathlib import Path
from typing import Dict

_SRC_MODULE_NAME = "chromium_src"

# List of extra libraries that should be available for importing
# directly as the module named in the key of this dictionary.
# Those are globally importable so they can potentially cause
# conflicts in names.
# TODO(crbug.com/488362708): Consider handling global imports through venv.
_EXTRA_MODULES: Dict[str, str] = {"typ": "third_party/catapult/third_party/typ"}


def setup_extra_modules(chromium_src_path: str):
  for import_path in _EXTRA_MODULES.values():
    if import_path in sys.path:
      continue
    sys.path.append(os.path.join(chromium_src_path, import_path))


def setup_chromium_src_module(chromium_src_path: str):
  """Sets up a chromium_src module linking to root src/

  This is done to allow importing by full path which is a recommended
  method for tools/metrics specifically"""
  # skip it if it was already set up
  if _SRC_MODULE_NAME in sys.modules:
    return

  chromium_root_path = str(Path(chromium_src_path).resolve())
  module = types.ModuleType(_SRC_MODULE_NAME)
  module.__path__ = [chromium_root_path]
  sys.modules[_SRC_MODULE_NAME] = module


def setup_modules(chromium_src_path: str):
  """Sets up modules required by tools/metrics"""

  setup_chromium_src_module(chromium_src_path)
  setup_extra_modules(chromium_src_path)

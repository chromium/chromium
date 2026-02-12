# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
from pathlib import Path
import types

SRC_MODULE_NAME = "chromium_src"

def setup_chromium_src_module(chromium_src_path: str):
  """Sets up a chromium_src module linking to root src/

  This is done to allow importing by full path which is a recommended
  method for tools/metrics specifically"""
  # skip it if it was already set up
  if SRC_MODULE_NAME in sys.modules:
    return

  chromium_root_path = str(Path(chromium_src_path).resolve())
  module = types.ModuleType(SRC_MODULE_NAME)
  module.__path__ = [chromium_root_path]
  sys.modules[SRC_MODULE_NAME] = module

def setup_modules(chromium_src_path: str):
  """Sets up modules required by tools/metrics"""

  setup_chromium_src_module(chromium_src_path)

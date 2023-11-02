# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Logic needed by multiple archive-related modules."""

import logging
import os
import re

import models


def ExtendSectionRange(section_range_by_name, section_name, delta_size):
  """Adds |delta_size| to |section_name|'s size in |section_range_by_name|."""
  prev_address, prev_size = section_range_by_name.get(section_name, (0, 0))
  section_range_by_name[section_name] = (prev_address, prev_size + delta_size)


def _NormalizeObjectPath(path):
  """Normalizes object paths.

  Prefixes are removed: obj/, ../../
  Archive names made more pathy: foo/bar.a(baz.o) -> foo/bar.a/baz.o
  """
  if path.startswith('obj/'):
    # Convert obj/third_party/... -> third_party/...
    path = path[4:]
  elif path.startswith('../../'):
    # Convert ../../third_party/... -> third_party/...
    path = path[6:]
  elif path.startswith('/'):
    # Convert absolute paths to $SYSTEM/basename.o.
    path = os.path.join(models.SYSTEM_PREFIX_PATH, os.path.basename(path))
  if path.endswith(')'):
    # Convert foo/bar.a(baz.o) -> foo/bar.a/baz.o so that hierarchical
    # breakdowns consider the .o part to be a separate node.
    start_idx = path.rindex('(')
    path = os.path.join(path[:start_idx], path[start_idx + 1:-1])
  return path


def _NormalizeSourcePath(path, gen_dir_pattern):
  """Returns (is_generated, normalized_path)"""
  # Don't change $APK/, or $NATIVE/ paths.
  if path.startswith('$'):
    return False, path
  if gen_dir_pattern:
    # Non-chromium gen_dir logic.
    m = gen_dir_pattern.match(path)
    if m:
      return True, path[m.end():]
    return False, path

  if path.startswith('gen/'):
    # Convert gen/third_party/... -> third_party/...
    return True, path[4:]
  if path.startswith('../../'):
    # Convert ../../third_party/... -> third_party/...
    return False, path[6:]
  if path.startswith('/'):
    # Convert absolute paths to $SYSTEM/basename.cpp.
    # E.g.: /buildbot/src/android/ndk-release-r23/toolchain/llvm-project/
    #       libcxx/src/vector.cpp
    path = os.path.join(models.SYSTEM_PREFIX_PATH, os.path.basename(path))
  return True, path


def NormalizePaths(raw_symbols, gen_dir_regex=None):
  """Fills in the |source_path| attribute and normalizes |object_path|."""
  logging.info('Normalizing source and object paths')
  gen_dir_pattern = re.compile(gen_dir_regex) if gen_dir_regex else None
  for symbol in raw_symbols:
    if symbol.object_path:
      symbol.object_path = _NormalizeObjectPath(symbol.object_path)
    if symbol.source_path:
      symbol.generated_source, symbol.source_path = _NormalizeSourcePath(
          symbol.source_path, gen_dir_pattern)

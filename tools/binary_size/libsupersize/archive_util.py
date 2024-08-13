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


def _NormalizeObjectPath(path, obj_prefixes):
  """Normalizes object paths.

  Prefixes are removed: obj/, ../../
  Archive names made more pathy: foo/bar.a(baz.o) -> foo/bar.a/baz.o
  """
  if path.startswith('../../'):
    # Convert ../../third_party/... -> third_party/...
    path = path[6:]
  elif path.startswith('/'):
    # Convert absolute paths to $SYSTEM/basename.o.
    path = os.path.join(models.SYSTEM_PREFIX_PATH, os.path.basename(path))
  else:
    # Convert obj/third_party/... -> third_party/...
    for prefix in obj_prefixes:
      if path.startswith(prefix):
        path = path[len(prefix):]

  if path.endswith(')'):
    # Convert foo/bar.a(baz.o) -> foo/bar.a/baz.o so that hierarchical
    # breakdowns consider the .o part to be a separate node.
    start_idx = path.rindex('(')
    path = os.path.join(path[:start_idx], path[start_idx + 1:-1])
  return path


def _NormalizeSourcePath(path, gen_prefixes, gen_dir_pattern):
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

  if path.startswith('../../'):
    # Convert ../../third_party/... -> third_party/...
    return False, path[6:]
  if path.startswith('/'):
    # Convert absolute paths to $SYSTEM/basename.cpp.
    # E.g.: /buildbot/src/android/ndk-release-r23/toolchain/llvm-project/
    #       libcxx/src/vector.cpp
    path = os.path.join(models.SYSTEM_PREFIX_PATH, os.path.basename(path))

  # Convert gen/third_party/... -> third_party/...
  for prefix in gen_prefixes:
    if path.startswith(prefix):
      return True, path[len(prefix):]

  return True, path


def NormalizePaths(raw_symbols, gen_dir_regex=None, toolchain_subdirs=None):
  """Fills in the |source_path| attribute and normalizes |object_path|."""
  logging.info('Normalizing source and object paths')
  gen_dir_pattern = re.compile(gen_dir_regex) if gen_dir_regex else None
  obj_prefixes = ['obj/']
  gen_prefixes = ['gen/']
  if toolchain_subdirs != None:
    obj_prefixes.extend(f'{t}/obj/' for t in toolchain_subdirs)
    gen_prefixes.extend(f'{t}/gen/' for t in toolchain_subdirs)
  for symbol in raw_symbols:
    if symbol.object_path:
      symbol.object_path = _NormalizeObjectPath(symbol.object_path,
                                                obj_prefixes)
    if symbol.source_path:
      symbol.generated_source, symbol.source_path = _NormalizeSourcePath(
          symbol.source_path, gen_prefixes, gen_dir_pattern)


def _ComputeAncestorPath(path_list, symbol_count):
  """Returns the common ancestor of the given paths."""
  if not path_list:
    return ''

  prefix = os.path.commonprefix(path_list)
  # Check if all paths were the same.
  if prefix == path_list[0]:
    return prefix

  # Put in buckets to cut down on the number of unique paths.
  if symbol_count >= 100:
    symbol_count_str = '100+'
  elif symbol_count >= 50:
    symbol_count_str = '50-99'
  elif symbol_count >= 20:
    symbol_count_str = '20-49'
  elif symbol_count >= 10:
    symbol_count_str = '10-19'
  else:
    symbol_count_str = str(symbol_count)

  # Put the path count as a subdirectory so that grouping by path will show
  # "{shared}" as a bucket, and the symbol counts as leafs.
  if not prefix:
    return os.path.join('{shared}', symbol_count_str)
  return os.path.join(os.path.dirname(prefix), '{shared}', symbol_count_str)


def CompactLargeAliasesIntoSharedSymbols(raw_symbols, max_count):
  """Converts symbols with large number of aliases into single symbols.

  The merged symbol's path fields are changed to common-ancestor paths in
  the form: common/dir/{shared}/$SYMBOL_COUNT

  Assumes aliases differ only by path (not by name).
  """
  num_raw_symbols = len(raw_symbols)
  num_shared_symbols = 0
  src_cursor = 0
  dst_cursor = 0
  while src_cursor < num_raw_symbols:
    symbol = raw_symbols[src_cursor]
    raw_symbols[dst_cursor] = symbol
    dst_cursor += 1
    aliases = symbol.aliases
    if aliases and len(aliases) > max_count:
      symbol.source_path = _ComputeAncestorPath(
          [s.source_path for s in aliases if s.source_path], len(aliases))
      symbol.object_path = _ComputeAncestorPath(
          [s.object_path for s in aliases if s.object_path], len(aliases))
      symbol.generated_source = all(s.generated_source for s in aliases)
      symbol.aliases = None
      num_shared_symbols += 1
      src_cursor += len(aliases)
    else:
      src_cursor += 1
  raw_symbols[dst_cursor:] = []
  num_removed = src_cursor - dst_cursor
  logging.debug('Converted %d aliases into %d shared-path symbols', num_removed,
                num_shared_symbols)


def RemoveAssetSuffix(path):
  """Undo asset path suffixing. https://crbug.com/357131361"""
  # E.g.: "assets/foo.pak+org.foo.bar+"
  if path.endswith('+'):
    suffix_idx = path.rfind('+', 0, len(path) - 1)
    if suffix_idx != -1:
      path = path[:suffix_idx]
  return path

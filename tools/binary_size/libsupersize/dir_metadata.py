# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helpers for determining Component of directories."""

import functools
import os
import re

_METADATA_FILENAME = 'DIR_METADATA'
_METADATA_COMPONENT_REGEX = re.compile(r'^\s*component:\s*"(.*?)"',
                                       re.MULTILINE)
_METADATA_MIXINS_REGEX = re.compile(r'^\s*mixins:\s*"(.*?)"', re.MULTILINE)
# Paths that are missing metadata, and where it's hard to add (e.g. code in
# other repositories.
_COMPONENT_DEFAULTS = {
    os.path.join('third_party', 'webrtc'): 'Blink>WebRTC',
    os.path.join('logging', 'rtc_event_log'): 'Blink>WebRTC',  # Generated files
    os.path.join('modules'): 'Blink>WebRTC',  # Generated files
}


def _SafeRead(path):
  try:
    with open(path) as f:
      return f.read()
  except IOError:
    # Need to catch both FileNotFoundError and NotADirectoryError since
    # source_paths for .aar files look like: /path/to/lib.aar/path/within/zip
    return ''


@functools.lru_cache
class _ComponentLookupContext:
  def __init__(self, source_directory, component_overrides):
    self._mixins_cache = {}
    self._dir_cache = _COMPONENT_DEFAULTS.copy()
    self._source_directory = source_directory
    self._component_overrides = component_overrides

  def ComponentForSourcePath(self, source_path):
    return self._ComponentForDirectory(os.path.dirname(source_path))

  def _ParseComponentFromMetadata(self, path):
    """Extracts Component from DIR_METADATA."""
    result = self._mixins_cache.get(path)
    if result is not None:
      return result

    data = _SafeRead(path)
    m = _METADATA_COMPONENT_REGEX.search(data)
    if m:
      result = m.group(1)
    else:
      # Recurse on mixins.
      self._mixins_cache[path] = ''  # Guard against cycles.
      result = ''
      for mixin_path in _METADATA_MIXINS_REGEX.findall(data):
        if mixin_path.startswith('//'):
          mixin_path = os.path.join(self._source_directory, mixin_path[2:])
        else:
          logging.warning('Found non-ablsolute mixin path in %s', path)
          continue
        result = self._ParseComponentFromMetadata(mixin_path)
        if result:
          break
    self._mixins_cache[path] = result
    return result

  def _ComponentForDirectory(self, directory):
    """Searches all parent directories for COMPONENT in OWNERS files.

    Args:
      directory: Path of directory to start searching from relative to
        |source_directory|.

    Returns:
      COMPONENT belonging to |path|, or empty string if not found.
    """
    assert not os.path.isabs(directory)
    component = self._dir_cache.get(directory)
    if component is not None:
      return component

    for prefix, component in self._component_overrides:
      if directory.startswith(prefix):
        result = component
        break
    else:
      metadata_path = os.path.join(self._source_directory, directory,
                                   _METADATA_FILENAME)
      result = self._ParseComponentFromMetadata(metadata_path)

    if not result:
      parent_directory = os.path.dirname(directory)
      if parent_directory:
        result = self._ComponentForDirectory(parent_directory)

    self._dir_cache[directory] = result
    return result


def PopulateComponents(raw_symbols, source_directory, component_overrides,
                       default_component):
  """Populates the |component| field based on |source_path|.

  Symbols without a |source_path| are skipped.

  Args:
    raw_symbols: list of Symbol objects.
    source_directory: Directory to use as the root.
    component_overrides: Tuple of (source path prefix, component) tuples.
    default_component: Component to use when none was found.
  """
  # Convert to tuple for lru_cache.
  context = _ComponentLookupContext(source_directory, component_overrides)
  for symbol in raw_symbols:
    found_component = ''
    if symbol.source_path:
      found_component = context.ComponentForSourcePath(symbol.source_path)
    if not found_component and symbol.object_path:
      # Some generated files and not put under their target_gen_dir (common
      # grit _resources_maps.cc files). So also look at object path.
      found_component = context.ComponentForSourcePath(symbol.object_path)
    symbol.component = found_component or default_component

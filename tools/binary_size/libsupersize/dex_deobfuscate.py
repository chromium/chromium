# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities to restore DEX symbols name from obfuscated names."""

import functools
import logging
import re

_PROGUARD_CLASS_MAPPING_RE = re.compile(r'(?P<original_name>[^ ]+)'
                                        r' -> '
                                        r'(?P<obfuscated_name>[^:]+):')


def _CreateClassDeobfuscationMap(mapping_line_iter):
  """Parses .mapping file lines into map to deobfuscate class names."""
  mapping = {}
  for line in mapping_line_iter:
    # We are on a class name so add it to the class mapping.
    if not line.startswith(' '):
      match = _PROGUARD_CLASS_MAPPING_RE.search(line)
      if match:
        mapping[match.group('obfuscated_name')] = match.group('original_name')
  return mapping


class CachedDexDeobfuscators:
  """Computes and caches obfuscators for DEX symbols."""

  @functools.lru_cache(None)
  def GetForMappingFile(self, proguard_mapping_file_path):
    """Gets a map to deobfuscate class names using .mapping file data.

    Args:
      proguard_mapping_file_path: Path to ProGuard .mapping file.

    Returns:
      A dict to map obfuscated class names to original names, or empty dict if
      input was given as None.
    """
    if proguard_mapping_file_path is None:
      return {}
    logging.debug('Parsing mapping file %s', proguard_mapping_file_path)
    with open(proguard_mapping_file_path, 'r') as fh:
      return _CreateClassDeobfuscationMap(fh)

# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test utilities for cygprofile scripts."""

import collections

import process_profiles

# Used by ProfileFile to generate unique file names.
_FILE_COUNTER = 0

SimpleTestSymbol = collections.namedtuple(
    'SimpleTestSymbol', ['name', 'offset', 'size'])


class TestSymbolOffsetProcessor(process_profiles.SymbolOffsetProcessor):
  def __init__(self, symbol_infos):
    super().__init__(None)
    self._symbol_infos = symbol_infos


class TestProfileManager(process_profiles.ProfileManager):
  def __init__(self, filecontents_mapping):
    super().__init__(filecontents_mapping.keys())
    self._filecontents_mapping = filecontents_mapping

  def _ReadOffsets(self, filename):
    return self._filecontents_mapping[filename]

  def _ReadJSON(self, filename):
    return self._filecontents_mapping[filename]


def ProfileFile(timestamp_sec, phase, process_name=None):
  global _FILE_COUNTER
  _FILE_COUNTER += 1
  if process_name:
    name_str = process_name + '-'
  else:
    name_str = ''
  return 'test-directory/profile-hitmap-{}{}-{}.txt_{}'.format(
      name_str, _FILE_COUNTER, timestamp_sec * 1000 * 1000 * 1000, phase)

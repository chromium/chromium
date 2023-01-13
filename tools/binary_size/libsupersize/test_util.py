# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import difflib
import logging
import os

import describe


_SCRIPT_DIR = os.path.dirname(__file__)
TEST_DATA_DIR = os.path.join(_SCRIPT_DIR, 'testdata')
_MOCK_TOOL_PREFIX = os.path.join(os.path.abspath(TEST_DATA_DIR),
                                 'mock_toolchain', '')
_MOCK_SDK_DIR = os.path.join(TEST_DATA_DIR, 'mock_sdk')
TEST_SOURCE_DIR = os.path.join(TEST_DATA_DIR, 'mock_source_directory')
TEST_OUTPUT_DIR = os.path.join(TEST_SOURCE_DIR, 'out', 'Release')


# Helper to make presubmit happy with .golden files.
def _Neutralize(line):
  return line + ' # nocheck' if '-master.' in line else line


class Golden:
  """Utility to use or manage "Golden" test files."""

  # Global state on whether to update Golden files in CheckOrUpdate().
  do_update = False

  @staticmethod
  def EnableUpdate():
    Golden.do_update = True

  @staticmethod
  def CheckOrUpdate(golden_path, actual_lines):
    if Golden.do_update:
      with open(golden_path, 'w') as file_obj:
        describe.WriteLines(map(_Neutralize, actual_lines), file_obj.write)
      logging.info('Wrote %s', golden_path)
    else:
      with open(golden_path) as file_obj:
        expected = list(file_obj)
        actual = list(_Neutralize(l) + '\n' for l in actual_lines)
        assert actual == expected, (
            ('Did not match %s.\n' % golden_path) + ''.join(
                difflib.unified_diff(expected, actual, 'expected', 'actual')))


@contextlib.contextmanager
def AddMocksToPath():
  prev_path = os.environ['PATH']
  os.environ['PATH'] = _MOCK_TOOL_PREFIX[:-1] + os.path.pathsep + prev_path
  os.environ['SUPERSIZE_APK_ANALYZER'] = os.path.join(_MOCK_SDK_DIR, 'tools',
                                                      'bin', 'apkanalyzer')
  os.environ['SUPERSIZE_AAPT2'] = os.path.join(_MOCK_SDK_DIR, 'tools', 'bin',
                                               'aapt2')
  os.environ['SUPERSIZE_TOOL_PREFIX'] = _MOCK_TOOL_PREFIX
  try:
    yield
  finally:
    del os.environ['SUPERSIZE_TOOL_PREFIX']
    del os.environ['SUPERSIZE_AAPT2']
    del os.environ['SUPERSIZE_APK_ANALYZER']
    os.environ['PATH'] = prev_path

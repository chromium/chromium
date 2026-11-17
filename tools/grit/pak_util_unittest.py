#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import sys
import tempfile
import unittest
from unittest import mock

import pak_util


class PackUtilTest(unittest.TestCase):
  def test_extract(self):
    tempdir = tempfile.mkdtemp()
    old_argv = sys.argv
    grit_root_dir = os.path.abspath(os.path.dirname(__file__))
    sys.argv = [
      'pak_util_unittest.py',
      'extract',
      os.path.join(grit_root_dir, 'grit/testdata/resources.pak'),
      '-o',
      tempdir,
    ]
    pak_util.main()
    sys.argv = old_argv
    shutil.rmtree(tempdir, ignore_errors=True)

  def test_maybe_decompress_zstd(self):
    if not pak_util._zstd and not shutil.which('zstd'):
      self.skipTest('Requires Python 3.14+ or zstd on PATH.')
    # "this is id 9" compressed with:
    # zstd -c --no-check --stream-size=12
    compressed = bytes.fromhex('28b52ffd200c610000746869732069732069642039')
    self.assertEqual(b'this is id 9', pak_util._MaybeDecompress(compressed))

  def test_maybe_decompress_zstd_cli(self):
    compressed = bytes.fromhex('28b52ffd200c610000746869732069732069642039')
    with mock.patch('subprocess.run') as mock_run:
      mock_run.return_value.stdout = b'this is id 9'
      self.assertEqual(
        b'this is id 9',
        pak_util._MaybeDecompress(compressed, zstd_path='/fake/zstd'),
      )
    mock_run.assert_called_once_with(
      ['/fake/zstd', '--decompress', '--stdout'],
      input=compressed,
      stdout=pak_util.subprocess.PIPE,
      check=True,
    )


if __name__ == '__main__':
  unittest.main()

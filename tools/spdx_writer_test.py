#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for //tools/spdx_writer.py."""

import os
import sys
import unittest

REPOSITORY_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
sys.path.append(os.path.join(REPOSITORY_ROOT, 'tools'))

from spdx_writer import _get_spdx_path
from spdx_writer import _Package

args = {
    'root': '/src',
    'pkg_name': 'mypkg',
    'root_license': '/src/LICENSE',
    'link_prefix': 'https://google.com',
    'doc_name': 'mydoc',
}


class SpdxPathTest(unittest.TestCase):
  def test_get_spdx_path(self):
    actual = _get_spdx_path('/src', '/src/root/third_party/abc')
    self.assertEqual(actual, '/root/third_party/abc')

  def test_get_spdx_path_error(self):
    with self.assertRaises(ValueError):
      _get_spdx_path('/src', '/some/other/path')


class PackageTest(unittest.TestCase):
  def setUp(self):
    super().setUp()

    self.p = _Package('abc def ghi', '/src/LICENSE')

  def test_package_spdx_id(self):
    self.assertEqual(self.p.package_spdx_id, 'SPDXRef-Package-abc-def-ghi')

  def test_license_spdx_id(self):
    self.assertEqual(self.p.license_spdx_id, 'LicenseRef-abc-def-ghi')


if __name__ == '__main__':
  unittest.main()

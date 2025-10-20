#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for //tools/spdx_writer.py."""

import collections
import os
import sys
import unittest

REPOSITORY_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.append(os.path.join(REPOSITORY_ROOT, 'tools', 'licenses'))

from spdx_writer import _get_spdx_path
from spdx_writer import _Package
from spdx_writer import _SPDXJSONWriter
from test_utils import path_from_root


class SpdxPathTest(unittest.TestCase):
  def test_get_spdx_path(self):
    actual = _get_spdx_path(path_from_root('src'),
                            path_from_root('src', 'root', 'third_party', 'abc'))
    self.assertEqual(actual, os.path.join(os.sep, 'root', 'third_party', 'abc'))

  def test_get_spdx_path_error(self):
    with self.assertRaises(ValueError):
      _get_spdx_path(path_from_root('src'),
                     path_from_root('some', 'other', 'path'))


class PackageTest(unittest.TestCase):
  def setUp(self):
    super().setUp()

    self.p = _Package('abc def ghi', path_from_root('src', 'LICENSE'))

  def test_package_spdx_id(self):
    self.assertEqual(self.p.package_spdx_id, 'SPDXRef-Package-abc-def-ghi')

  def test_license_spdx_id(self):
    self.assertEqual(self.p.license_spdx_id, 'LicenseRef-abc-def-ghi')


class SPDXJSONWriterTest(unittest.TestCase):
  def setUp(self):
    super().setUp()

    root_pkg = _Package('root', path_from_root('src', 'LICENSE'))
    self.writer = _SPDXJSONWriter(path_from_root('src'), root_pkg, '', '',
                                  '', lambda _: '')

  def test_get_dedup_id(self):
    id_dict = collections.defaultdict(int)
    elem_id = 'abc'

    id1 = self.writer._get_dedup_id(elem_id, id_dict)
    self.assertEqual(id1, elem_id)

    id2 = self.writer._get_dedup_id(elem_id, id_dict)
    self.assertEqual(id2, elem_id + '-1')

  def test_get_license_id(self):
    license_path = path_from_root('src', 'p1', 'LICENSE')
    p1 = _Package('p1', license_path)

    p1_id, need_license = self.writer._get_license_id(p1)
    self.assertEqual(p1_id, p1.license_spdx_id)
    self.assertTrue(need_license)

    # Try a new package with the same license path.
    p2 = _Package('p2', license_path)
    p2_id, need_license = self.writer._get_license_id(p2)
    self.assertEqual(p2_id, p1.license_spdx_id)
    self.assertFalse(need_license)


if __name__ == '__main__':
  unittest.main()

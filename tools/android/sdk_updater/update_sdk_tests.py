#! /usr/bin/env python
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import tempfile
import unittest

import update_sdk


class ChangeVersionInGNITests(unittest.TestCase):

  def setUp(self):
    super(ChangeVersionInGNITests, self).setUp()
    self._temp_dir = tempfile.mkdtemp()
    self._gni_file_path = os.path.join(self._temp_dir, 'test_file.gni')

  def tearDown(self):
    shutil.rmtree(self._temp_dir)
    super(ChangeVersionInGNITests, self).tearDown()

  def testBasic(self):
    with open(self._gni_file_path, 'w') as gni_file:
      gni_file.write('sample_gn_version_var = "1.2.3.4"')
    package = 'sample_package'
    arg_version = '2.3.4.5'
    gn_args_dict = {package: 'sample_gn_version_var'}
    update_sdk.ChangeVersionInGNI(package, arg_version, gn_args_dict,
                                  self._gni_file_path, False)
    with open(self._gni_file_path, 'r') as gni_file:
      self.assertEqual('sample_gn_version_var = "2.3.4.5"',
                       gni_file.read().strip())

  def testNoQuotes(self):
    with open(self._gni_file_path, 'w') as gni_file:
      gni_file.write('sample_gn_version_var = 1234')
    package = 'sample_package'
    arg_version = '2345'
    gn_args_dict = {package: 'sample_gn_version_var'}
    update_sdk.ChangeVersionInGNI(package, arg_version, gn_args_dict,
                                  self._gni_file_path, False)
    with open(self._gni_file_path, 'r') as gni_file:
      self.assertEqual('sample_gn_version_var = 2345', gni_file.read().strip())


if __name__ == '__main__':
  unittest.main()

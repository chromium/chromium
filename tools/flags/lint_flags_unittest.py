#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import pathlib
import unittest

import lint_flags
from pyfakefs import fake_filesystem_unittest


class FindUnusedFlagsTest(fake_filesystem_unittest.TestCase):

  def setUp(self):
    self.setUpPyfakefs()
    self.root_path = pathlib.Path() / 'fake' / 'chromium' / 'src'
    self.fs.create_dir(self.root_path)

  def test_find_unused_flags(self):
    metadata_path = self.root_path / 'chrome' / 'browser' / 'flag-metadata.json'
    metadata = [{
        'name': 'used-flag'
    }, {
        'name': 'unused-flag'
    }, {
        'name': 'enable-benchmarking'
    }, {
        'name': 'used-in-ios'
    }, {
        'name': 'used-in-site-isolation'
    }]
    self.fs.create_file(metadata_path, contents=json.dumps(metadata))

    path = self.root_path / 'chrome' / 'browser' / 'about_flags.cc'
    self.fs.create_file(path, contents='{"used-flag", "ignore-this"},\n')
    path = (self.root_path / 'ios' / 'chrome' / 'browser' / 'flags' /
            'about_flags.mm')
    self.fs.create_file(path, contents='\n\n{"used-in-ios", ...},\n')
    path = (self.root_path / 'chrome' / 'browser' / 'site_isolation' /
            'about_flags.h')
    self.fs.create_file(path, contents='"used-in-site-isolation"')

    unused_flags = lint_flags.find_unused_flags_in_metadata(self.root_path)
    self.assertEqual({'unused-flag'}, unused_flags)


if __name__ == '__main__':
  unittest.main()

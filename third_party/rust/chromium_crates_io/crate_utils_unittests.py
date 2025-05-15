#!/usr/bin/env vpython3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from crate_utils import (
    CHROMIUM_DIR,
    ConvertCrateIdToCrateEpoch,
    ConvertCrateIdToCrateName,
    ConvertCrateIdToCrateVersion,
    ConvertCrateIdToBuildDir,
    ConvertCrateIdToGnLabel,
    ConvertCrateIdToVendorDir,
    GetCurrentCrateIds,
)


class CrateUtilsTests(unittest.TestCase):

    def testGetCurrentCrateIds(self):
        crate_ids = GetCurrentCrateIds()
        self.assertTrue(crate_ids)

    def testConvertCrateIdToCrateEpoch(self):
        self.assertEqual(ConvertCrateIdToCrateEpoch("foo@0.1.2"), "v0_1")
        self.assertEqual(ConvertCrateIdToCrateEpoch("foo@1.2.3"), "v1")

    def testConvertCrateIdToBuildDir(self):
        actual_dir = ConvertCrateIdToBuildDir("foo-bar@1.2.3")
        expected_suffix = os.path.join("third_party", "rust", "foo_bar", "v1")
        self.assertEqual(actual_dir, os.path.join(CHROMIUM_DIR,
                                                  expected_suffix))

        actual_dir = ConvertCrateIdToBuildDir("bar_baz@0.12.3")
        expected_suffix = os.path.join("third_party", "rust", "bar_baz",
                                       "v0_12")
        self.assertEqual(actual_dir, os.path.join(CHROMIUM_DIR,
                                                  expected_suffix))

    def testConvertCrateIdToGnLabel(self):
        self.assertEqual(ConvertCrateIdToGnLabel("foo-bar@1.2.3"),
                         "//third_party/rust/foo_bar/v1:lib")
        self.assertEqual(ConvertCrateIdToGnLabel("bar_baz@0.12.3"),
                         "//third_party/rust/bar_baz/v0_12:lib")

    def testConvertCrateIdToVendorDir(self):
        actual_dir = ConvertCrateIdToVendorDir("foo-bar@1.2.3")
        expected_suffix = os.path.join("third_party", "rust",
                                       "chromium_crates_io", "vendor",
                                       "foo-bar-v1")
        self.assertEqual(actual_dir, os.path.join(CHROMIUM_DIR,
                                                  expected_suffix))

        actual_dir = ConvertCrateIdToVendorDir("bar_baz@0.12.3")
        expected_suffix = os.path.join("third_party", "rust",
                                       "chromium_crates_io", "vendor",
                                       "bar_baz-v0_12")
        self.assertEqual(actual_dir, os.path.join(CHROMIUM_DIR,
                                                  expected_suffix))

    def testConvertCrateIdToCrateName(self):
        self.assertEqual(ConvertCrateIdToCrateName("foo-bar@1.2.3"), "foo-bar")

    def testConvertCrateIdToCrateVersion(self):
        self.assertEqual(ConvertCrateIdToCrateVersion("foo-bar@1.2.3"), "1.2.3")


if __name__ == '__main__':
    unittest.main()

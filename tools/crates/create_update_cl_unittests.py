#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from create_update_cl import (
    CHROMIUM_DIR,
    ConvertCrateIdToCrateName,
    ConvertCrateIdToCrateVersion,
    ConvertCrateIdToEpochDir,
    ConvertCrateIdToGnLabel,
    ConvertCrateIdToVendorDir,
    CreateCommitDescription,
    CreateCommitTitle,
    DiffCrateIds,
    GetEpoch,
    SortedMarkdownList,
)

class DiffCrateIdsTests(unittest.TestCase):

    def testBasicsViaMinorUpdateDetection(self):
        before = set(
            ["multi@1.0.1", "multi@2.0.1", "deleted@3.0.1", "single@4.0.1"])
        after = set(
            ["multi@1.0.1", "multi@2.0.2", "added@5.0.1", "single@4.0.2"])
        diff = DiffCrateIds(before, after, only_minor_updates=True)
        self.assertEqual(diff.added_crate_ids, ["added@5.0.1"])
        self.assertEqual(diff.removed_crate_ids, ["deleted@3.0.1"])
        self.assertEqual(len(diff.updates), 2)
        self.assertEqual(diff.updates[0].old_crate_id, "multi@2.0.1")
        self.assertEqual(diff.updates[0].new_crate_id, "multi@2.0.2")
        self.assertEqual(diff.updates[1].old_crate_id, "single@4.0.1")
        self.assertEqual(diff.updates[1].new_crate_id, "single@4.0.2")

    def testMajorUpdateDetection(self):
        before = set(["foo@1.0.1"])
        after = set(["foo@2.0.2"])
        diff = DiffCrateIds(before, after, only_minor_updates=False)
        self.assertEqual(diff.added_crate_ids, [])
        self.assertEqual(diff.removed_crate_ids, [])
        self.assertEqual(len(diff.updates), 1)
        self.assertEqual(diff.updates[0].old_crate_id, "foo@1.0.1")
        self.assertEqual(diff.updates[0].new_crate_id, "foo@2.0.2")

    def testMajorUpdateWithOnlyMinorUpdateDetection(self):
        before = set(["foo@1.0.1"])
        after = set(["foo@2.0.2"])
        diff = DiffCrateIds(before, after, only_minor_updates=True)
        self.assertEqual(diff.added_crate_ids, ["foo@2.0.2"])
        self.assertEqual(diff.removed_crate_ids, ["foo@1.0.1"])
        self.assertEqual(diff.updates, [])

    def testNoChanges(self):
        before = set(["foo@1.0.1"])
        after = set(["foo@1.0.1"])
        diff = DiffCrateIds(before, after, only_minor_updates=True)
        self.assertEqual(diff.added_crate_ids, [])
        self.assertEqual(diff.removed_crate_ids, [])
        self.assertEqual(diff.updates, [])


class CommitDescriptionTests(unittest.TestCase):

    def testTitle(self):
        before = set(["updated_crate@2.0.1", "deleted@3.0.1"])
        after = set(["updated_crate@2.0.2", "added@5.0.1"])
        diff = DiffCrateIds(before, after, only_minor_updates=True)
        actual_title = CreateCommitTitle("updated_crate@2.0.1", diff)
        expected_title = \
             "Roll updated_crate: 2.0.1 => 2.0.2 in //third_party/rust."
        self.assertEqual(actual_title, expected_title)

    def testFullDescription(self):
        before = set(["updated_crate@2.0.1", "deleted@3.0.1"])
        after = set(["updated_crate@2.0.2", "added@5.0.1"])
        diff = DiffCrateIds(before, after, only_minor_updates=True)

        actual_desc = CreateCommitDescription("Commit title.", diff, False)
        expected_desc = \
"""Commit title.

This CL has been created semi-automatically.  The expected review
process and other details can be found at
//tools/crates/create_update_cl.md

Updated crates:

* updated_crate: 2.0.1 => 2.0.2

New crates:

* added@5.0.1

Removed crates:

* deleted@3.0.1

Bug: None
Cq-Include-Trybots: chromium/try:android-rust-arm32-rel
Cq-Include-Trybots: chromium/try:android-rust-arm64-dbg
Cq-Include-Trybots: chromium/try:android-rust-arm64-rel
Cq-Include-Trybots: chromium/try:linux-rust-x64-dbg
Cq-Include-Trybots: chromium/try:linux-rust-x64-rel
Cq-Include-Trybots: chromium/try:win-rust-x64-dbg
Cq-Include-Trybots: chromium/try:win-rust-x64-rel
Disable-Rts: True
"""
        self.assertEqual(actual_desc, expected_desc)


class OtherTests(unittest.TestCase):

    def testGetEpoch(self):
        self.assertEqual(GetEpoch("0.1.2"), "v0_1")
        self.assertEqual(GetEpoch("1.2.3"), "v1")

    def testConvertCrateIdToEpochDir(self):
        actual_dir = ConvertCrateIdToEpochDir("foo-bar@1.2.3")
        expected_suffix = os.path.join("third_party", "rust", "foo_bar", "v1")
        self.assertEqual(actual_dir, os.path.join(CHROMIUM_DIR,
                                                  expected_suffix))

        actual_dir = ConvertCrateIdToEpochDir("bar_baz@0.12.3")
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
                                       "foo-bar-1.2.3")
        self.assertEqual(actual_dir, os.path.join(CHROMIUM_DIR,
                                                  expected_suffix))

        actual_dir = ConvertCrateIdToVendorDir("bar_baz@0.12.3")
        expected_suffix = os.path.join("third_party", "rust",
                                       "chromium_crates_io", "vendor",
                                       "bar_baz-0.12.3")
        self.assertEqual(actual_dir, os.path.join(CHROMIUM_DIR,
                                                  expected_suffix))

    def testConvertCrateIdToCrateName(self):
        self.assertEqual(ConvertCrateIdToCrateName("foo-bar@1.2.3"), "foo-bar")

    def testConvertCrateIdToCrateVersion(self):
        self.assertEqual(ConvertCrateIdToCrateVersion("foo-bar@1.2.3"), "1.2.3")

    def testSortedMarkdownList(self):
        input = ["bbb " * 25, "aaa " * 30, "ccc " * 35]
        actual_output = SortedMarkdownList(input)
        expected_output = \
"""* aaa aaa aaa aaa aaa aaa aaa aaa aaa aaa aaa aaa aaa aaa aaa aaa aaa
  aaa aaa aaa aaa aaa aaa aaa aaa aaa aaa aaa aaa aaa
* bbb bbb bbb bbb bbb bbb bbb bbb bbb bbb bbb bbb bbb bbb bbb bbb bbb
  bbb bbb bbb bbb bbb bbb bbb bbb
* ccc ccc ccc ccc ccc ccc ccc ccc ccc ccc ccc ccc ccc ccc ccc ccc ccc
  ccc ccc ccc ccc ccc ccc ccc ccc ccc ccc ccc ccc ccc ccc ccc ccc ccc
  ccc"""
        self.assertEqual(actual_output, expected_output)


if __name__ == '__main__':
    unittest.main()

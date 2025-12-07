#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from create_update_cl import (
    CreateCommitDescription,
    CreateCommitTitle,
    CreateCommitTitleForBreakingUpdate,
    DiffCrateIds,
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
        actual_title = CreateCommitTitle("updated_crate@2.0.1",
                                         "updated_crate@2.0.2")
        expected_title = \
             "Roll updated_crate: 2.0.1 => 2.0.2 in //third_party/rust."
        self.assertEqual(actual_title, expected_title)

    def testBreakingUpdateTitle(self):
        before = set(["updated_foo@2.0.1", "updated_bar@3.0.1"])
        after = set(["updated_foo@3.0.2", "updated_bar@4.0.2"])
        diff = DiffCrateIds(before, after, only_minor_updates=False)
        actual_title = CreateCommitTitleForBreakingUpdate(diff)
        expected_title = \
              "Roll updated_bar: 3.0.1 => 4.0.2, updated_foo: 2.0.1 => 3.0.2"
        self.assertEqual(actual_title, expected_title)

    def testBreakingUpdateTitleLong(self):
        before = set(
            ["updated_foo@2.0.1", "updated_bar@3.0.1", "updated_baz@4.0.1"])
        after = set(
            ["updated_foo@3.0.2", "updated_bar@4.0.2", "updated_baz@5.0.2"])
        diff = DiffCrateIds(before, after, only_minor_updates=False)
        actual_title = CreateCommitTitleForBreakingUpdate(diff)
        expected_title = \
             "Roll updated_bar: 3.0.1 => 4.0.2, updated_baz: 4.0.1 => 5.0.2,..."
        self.assertEqual(actual_title, expected_title)

    def testFullDescription(self):
        before = set(["updated_crate@2.0.1", "deleted@3.0.1"])
        after = set(["updated_crate@2.0.2", "added@5.0.1"])
        diff = DiffCrateIds(before, after, only_minor_updates=True)

        actual_desc = CreateCommitDescription("Commit title.", diff)
        expected_desc = \
"""Commit title.

This CL has been created semi-automatically.  The expected review
process and other details can be found at
//tools/crates/create_update_cl.md

Updated crates:

* updated_crate: 2.0.1 => 2.0.2;
  https://docs.rs/crate/updated_crate/2.0.2

New crates:

* added@5.0.1; https://docs.rs/crate/added/5.0.1

Removed crates:

* deleted@3.0.1; https://docs.rs/crate/deleted/3.0.1

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

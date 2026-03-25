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

from unittest.mock import MagicMock, patch
import create_update_cl

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


class AutoUpdateTests(unittest.TestCase):

    @patch('create_update_cl.UpdateCrate')
    @patch('create_update_cl.FindDiffOfCrateUpdate')
    @patch('create_update_cl.FindUpdateableCrates')
    @patch('create_update_cl.CheckoutInitialBranch')
    @patch('create_update_cl.GetMissingCrates')
    @patch('create_update_cl.DoArgsAskForBreakingChanges')
    def testSmartChaining(self, mock_breaking, mock_missing, mock_checkout,
                          mock_find_updates, mock_find_diff, mock_update_crate):
        # Setup: Three crates to update.
        # Crate 1 (size 1) - affects crate1 and shared-crate.
        # Crate 2 (size 2) - affects only crate2.
        # Crate 3 (size 3) - affects crate3 and shared-crate.
        # The sizes (1, 2, 3) ensure the processing order is C1, C2, C3.

        mock_find_updates.return_value = [
            ('crate1@1.0', 'crate1@1.1'),
            ('crate2@1.0', 'crate2@1.1'),
            ('crate3@1.0', 'crate3@1.1'),
        ]
        mock_missing.return_value = []
        mock_breaking.return_value = False

        diff1 = create_update_cl.CratesDiff(updates=[
            create_update_cl.UpdatedCrate('crate1@1.0', 'crate1@1.1'),
            create_update_cl.UpdatedCrate('shared-crate@1.0',
                                          'shared-crate@1.1')
        ],
                                            removed_crate_ids=[],
                                            added_crate_ids=[])

        diff2 = create_update_cl.CratesDiff(
            updates=[create_update_cl.UpdatedCrate('crate2@1.0', 'crate2@1.1')],
            removed_crate_ids=[],
            added_crate_ids=['d_a', 'd_b', 'd_c'])  # diff2 size is 4.

        diff3 = create_update_cl.CratesDiff(
            updates=[
                create_update_cl.UpdatedCrate('crate3@1.0', 'crate3@1.1'),
                create_update_cl.UpdatedCrate('shared-crate@1.0',
                                              'shared-crate@1.1')
            ],
            removed_crate_ids=[],
            added_crate_ids=['d1', 'd2', 'd3', 'd4', 'd5'])  # diff3 size is 7.

        diffs = {
            'crate1@1.0': diff1,
            'crate2@1.0': diff2,
            'crate3@1.0': diff3,
        }

        # Ensure sizes are 2, 2, 4. Let's adjust diff1 to have size 1.
        diff1.updates = [
            create_update_cl.UpdatedCrate('crate1@1.0', 'crate1@1.1')
        ]
        # Wait, if diff1 doesn't have shared-crate, Crate 3 won't chain.
        # Let's use added_crate_ids to control size instead.
        diff1.updates = [
            create_update_cl.UpdatedCrate('crate1@1.0', 'crate1@1.1'),
            create_update_cl.UpdatedCrate('shared-crate@1.0',
                                          'shared-crate@1.1')
        ]
        # diff1 size is 2.
        diff2.updates = [
            create_update_cl.UpdatedCrate('crate2@1.0', 'crate2@1.1')
        ]
        diff2.added_crate_ids = ['d_a', 'd_b', 'd_c']  # diff2 size is 4.
        diff3.updates = [
            create_update_cl.UpdatedCrate('crate3@1.0', 'crate3@1.1'),
            create_update_cl.UpdatedCrate('shared-crate@1.0',
                                          'shared-crate@1.1')
        ]
        diff3.added_crate_ids = ['d1', 'd2', 'd3', 'd4',
                                 'd5']  # diff3 size is 7.
        # Sizes: C1=2, C2=4, C3=7. Order will be C1, C2, C3.

        mock_find_diff.side_effect = lambda old_id, new_id, minor: diffs[old_id]

        def update_crate_side_effect(args, old_id, new_id, upstream,
                                     branch_num):
            return f"branch-for-{old_id}", diffs[old_id]

        mock_update_crate.side_effect = update_crate_side_effect

        args = MagicMock()
        args.upstream_branch = 'origin/main'
        args.chained = False
        args.remaining_args = []
        args.skip = []
        args.upload = False

        create_update_cl.AutoUpdate(args)

        # 1. Crate 1 (C1) is processed first. Upstream should be origin/main.
        self.assertEqual(mock_update_crate.call_args_list[0][0][3],
                         'origin/main')
        # 2. Crate 2 (C2) is processed second. Upstream should be origin/main
        # (no overlap).
        self.assertEqual(mock_update_crate.call_args_list[1][0][3],
                         'origin/main')
        # 3. Crate 3 (C3) is processed third. It overlaps with C1 via
        # 'shared-crate@1.0'.
        # Upstream should be branch-for-crate1@1.0.
        self.assertEqual(mock_update_crate.call_args_list[2][0][3],
                         'branch-for-crate1@1.0')


if __name__ == '__main__':
    unittest.main()

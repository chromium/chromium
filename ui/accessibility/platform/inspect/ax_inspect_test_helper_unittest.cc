// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_inspect_test_helper.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace ui {

TEST(AXInspectTestHelperTest, TestDiffLines) {
  // Files with the same lines should have an empty diff.
  EXPECT_THAT(AXInspectTestHelper::DiffLines({"broccoli", "turnip"},
                                             {"broccoli", "turnip"}),
              ElementsAre());

  // Empty files should have an empty diff.
  EXPECT_THAT(AXInspectTestHelper::DiffLines({}, {}), ElementsAre());

  // If one line differs, that line number should be returned as the diff.
  EXPECT_THAT(AXInspectTestHelper::DiffLines({"broccoli", "turnip"},
                                             {"watermelon", "turnip"}),
              ElementsAre(0));

  EXPECT_THAT(AXInspectTestHelper::DiffLines({"broccoli", "turnip"},
                                             {"broccoli", "rutabaga"}),
              ElementsAre(1));

  // Multiple lines can differ.
  EXPECT_THAT(AXInspectTestHelper::DiffLines(
                  {"broccoli", "turnip", "carrot", "squash"},
                  {"broccoli", "aspartame", "carrot", "toothbrush"}),
              ElementsAre(1, 3));

  // Blank lines in the expected file are allowed.
  EXPECT_THAT(AXInspectTestHelper::DiffLines(
                  {"", "broccoli", "turnip", "carrot", "", "squash"},
                  {"broccoli", "turnip", "carrot", "squash"}),
              ElementsAre());

  // Comments in the expected file are allowed.
  EXPECT_THAT(
      AXInspectTestHelper::DiffLines(
          {"#Vegetables:", "broccoli", "turnip", "carrot", "", "squash"},
          {"broccoli", "turnip", "carrot", "squash"}),
      ElementsAre());

  // Comments or blank lines in the actual file are NOT allowed, that's a diff.
  EXPECT_THAT(
      AXInspectTestHelper::DiffLines(
          {"broccoli", "turnip", "carrot", "squash"},
          {"#vegetables", "broccoli", "turnip", "carrot", "", "squash"}),
      ElementsAre(0, 1, 2, 3, 4));

  // If the expected file has an extra line, that's a diff.
  EXPECT_THAT(AXInspectTestHelper::DiffLines({"broccoli", "turnip", "cow"},
                                             {"broccoli", "turnip"}),
              ElementsAre(2));

  // If the actual file has an extra line, that's a diff.
  EXPECT_THAT(AXInspectTestHelper::DiffLines({"broccoli", "turnip"},
                                             {"broccoli", "turnip", "horse"}),
              ElementsAre(2));

  // If the expected file has an extra blank line, that's okay.
  EXPECT_THAT(AXInspectTestHelper::DiffLines({"broccoli", "turnip", ""},
                                             {"broccoli", "turnip"}),
              ElementsAre());
}

}  // namespace ui

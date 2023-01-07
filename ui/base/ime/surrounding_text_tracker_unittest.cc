// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/surrounding_text_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/composition_text.h"

namespace ui {

TEST(SurroundingTextTracker, SetCompositionText) {
  SurroundingTextTracker tracker;

  ui::CompositionText composition;
  composition.text = u"abc";
  composition.selection = gfx::Range(3);  // at the end.

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"abc", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(3), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(0, 3), tracker.predicted_state().composition);

  composition.text = u"xyzw";
  composition.selection = gfx::Range(4);  // at the end.

  tracker.OnSetCompositionText(composition);
  EXPECT_EQ(u"xyzw", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(4), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(0, 4), tracker.predicted_state().composition);
}

TEST(SurroundingTextTracker, SetCompositionTextWithExistingText) {
  SurroundingTextTracker tracker;
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg",
                           gfx::Range(3)));  // Set cursor between c and d.

  ui::CompositionText composition;
  composition.text = u"xyz";
  composition.selection = gfx::Range(3);  // at the end.

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(6), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(3, 6), tracker.predicted_state().composition);

  composition.text = u"pqrst";
  composition.selection = gfx::Range(0);  // at beginning.

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"abcpqrstdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(3), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(3, 8), tracker.predicted_state().composition);
}

TEST(SurroundingTextTracker, SetCompositionFromExistingText) {
  SurroundingTextTracker tracker;
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg",
                           gfx::Range(3)));  // Set cursor between c and d.
  tracker.OnSetCompositionFromExistingText(gfx::Range(3, 5));
  EXPECT_EQ(u"abcdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(3), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(3, 5), tracker.predicted_state().composition);
}

TEST(SurroundingTextTracker, ConfirmCompositionText) {
  SurroundingTextTracker tracker;
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg",
                           gfx::Range(3)));  // Set cursor between c and d.

  ui::CompositionText composition;
  composition.text = u"xyz";
  composition.selection = gfx::Range(1);  // between x and y.

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(4), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(3, 6), tracker.predicted_state().composition);

  tracker.OnConfirmCompositionText(/*keep_selection=*/false);

  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(6), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());

  // Nothing happens if no composition exists.
  tracker.OnConfirmCompositionText(/*keep_selection=*/false);
  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(6), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
}

TEST(SurroundingTextTracker, ConfirmCompositionTextWithKeepSelection) {
  SurroundingTextTracker tracker;
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg",
                           gfx::Range(3)));  // Set cursor between c and d.

  ui::CompositionText composition;
  composition.text = u"xyz";
  composition.selection = gfx::Range(1);  // between x and y.

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(4), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(3, 6), tracker.predicted_state().composition);

  tracker.OnConfirmCompositionText(/*keep_selection=*/true);

  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(4), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());

  // Nothing happens if no composition exists.
  tracker.OnConfirmCompositionText(/*keep_selection=*/true);
  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(4), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
}

TEST(SurroundingTextTracker, ClearCompositionText) {
  SurroundingTextTracker tracker;
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg",
                           gfx::Range(3)));  // Set cursor between c and d.

  ui::CompositionText composition;
  composition.text = u"xyz";
  composition.selection = gfx::Range(1);  // between x and y.

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(4), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(3, 6), tracker.predicted_state().composition);

  tracker.OnClearCompositionText();

  EXPECT_EQ(u"abcdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(3), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());

  // Set "cd" as composition text.
  tracker.OnSetCompositionFromExistingText(gfx::Range(2, 4));
  EXPECT_EQ(u"abcdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(3), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(2, 4), tracker.predicted_state().composition);

  // Then clear it again.
  tracker.OnClearCompositionText();

  EXPECT_EQ(u"abefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(2), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());

  // Nothing should happen if there's no composition.
  tracker.OnClearCompositionText();
  EXPECT_EQ(u"abefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(2), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
}

TEST(SurroundingTextTracker, InsertText) {
  SurroundingTextTracker tracker;

  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg",
                           gfx::Range(3)));  // Set cursor between c and d.

  tracker.OnInsertText(
      u"xyz", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(6), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  EXPECT_EQ(SurroundingTextTracker::UpdateResult::kUpdated,
            tracker.Update(u"abcxyzdefg", gfx::Range(6)));

  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg",
                           gfx::Range(3)));  // Set cursor between c and d.

  tracker.OnInsertText(
      u"xyz", TextInputClient::InsertTextCursorBehavior::kMoveCursorBeforeText);
  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(3), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  EXPECT_EQ(SurroundingTextTracker::UpdateResult::kUpdated,
            tracker.Update(u"abcxyzdefg", gfx::Range(3)));

  ASSERT_EQ(
      SurroundingTextTracker::UpdateResult::kReset,
      tracker.Update(u"abcdefg", gfx::Range(3, 4)));  // Set selection on "d".

  tracker.OnInsertText(
      u"xyz", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(u"abcxyzefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(6), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  EXPECT_EQ(SurroundingTextTracker::UpdateResult::kUpdated,
            tracker.Update(u"abcxyzefg", gfx::Range(6)));

  ASSERT_EQ(
      SurroundingTextTracker::UpdateResult::kReset,
      tracker.Update(u"abcdefg", gfx::Range(3, 4)));  // Set selection on "d".

  tracker.OnInsertText(
      u"xyz", TextInputClient::InsertTextCursorBehavior::kMoveCursorBeforeText);
  // 'd' should be replaced.
  EXPECT_EQ(u"abcxyzefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(3), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  EXPECT_EQ(SurroundingTextTracker::UpdateResult::kUpdated,
            tracker.Update(u"abcxyzefg", gfx::Range(3)));
}

TEST(SurroundingTextTracker, InsertTextWithComposition) {
  // Aliases to just the test data shorter.
  constexpr auto kMoveCursorBeforeText =
      TextInputClient::InsertTextCursorBehavior::kMoveCursorBeforeText;
  constexpr auto kMoveCursorAfterText =
      TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText;

  constexpr struct {
    gfx::Range selection;
    TextInputClient::InsertTextCursorBehavior cursor_behavior;
    const char16_t* expected_surrounding_text;
    gfx::Range expected_selection;
  } kTestData[] = {
      // Cursor between 'a' and 'b'.
      {gfx::Range(1), kMoveCursorBeforeText, u"axyzbfg", gfx::Range(1)},
      {gfx::Range(1), kMoveCursorAfterText, u"axyzbfg", gfx::Range(4)},

      // Selection of 'a'.
      {gfx::Range(0, 1), kMoveCursorBeforeText, u"xyzbfg", gfx::Range(0)},
      {gfx::Range(0, 1), kMoveCursorAfterText, u"xyzbfg", gfx::Range(3)},

      // Selection of "bc" (crossing the starting boundary of the composition).
      {gfx::Range(1, 3), kMoveCursorBeforeText, u"axyzfg", gfx::Range(1)},
      {gfx::Range(1, 3), kMoveCursorAfterText, u"axyzfg", gfx::Range(4)},

      // Cursor between 'c' and 'd' (inside composition).
      {gfx::Range(3), kMoveCursorBeforeText, u"abxyzfg", gfx::Range(2)},
      {gfx::Range(3), kMoveCursorAfterText, u"abxyzfg", gfx::Range(5)},

      // Selection of 'd' (inside composition).
      {gfx::Range(3, 4), kMoveCursorBeforeText, u"abxyzfg", gfx::Range(2)},
      {gfx::Range(3, 4), kMoveCursorAfterText, u"abxyzfg", gfx::Range(5)},

      // Selection of "ef" (crossing the end boundary of the composition).
      {gfx::Range(4, 6), kMoveCursorBeforeText, u"abxyzg", gfx::Range(2)},
      {gfx::Range(4, 6), kMoveCursorAfterText, u"abxyzg", gfx::Range(5)},

      // Cursor between 'f' and 'g'.
      {gfx::Range(6), kMoveCursorBeforeText, u"abfxyzg", gfx::Range(3)},
      {gfx::Range(6), kMoveCursorAfterText, u"abfxyzg", gfx::Range(6)},
  };

  for (const auto& test_case : kTestData) {
    SurroundingTextTracker tracker;

    ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
              tracker.Update(u"abcdefg", test_case.selection));

    // Set composition on "cde".
    tracker.OnSetCompositionFromExistingText(gfx::Range(2, 5));

    // Then insert text.
    tracker.OnInsertText(u"xyz", test_case.cursor_behavior);

    // Verification.
    EXPECT_EQ(test_case.expected_surrounding_text,
              tracker.predicted_state().surrounding_text);
    EXPECT_EQ(test_case.expected_selection,
              tracker.predicted_state().selection);
    EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  }
}

TEST(SurroundingTextTracker, ExtendSelectionAndDelete) {
  constexpr struct {
    gfx::Range selection;
    gfx::Range composition;
    size_t before;
    size_t after;
    const char16_t* expected_surrounding_text;
    gfx::Range expected_selection;
  } kTestData[] = {
      // null deletion.
      {gfx::Range(3), gfx::Range(), 0, 0, u"abcdefg", gfx::Range(3)},

      // Remove 1 char before the cursor.
      {gfx::Range(3), gfx::Range(), 1, 0, u"abdefg", gfx::Range(2)},

      // Remove 1 char after the cursor.
      {gfx::Range(3), gfx::Range(), 0, 1, u"abcefg", gfx::Range(3)},

      // Remove 1 char for each before and after the cursor.
      {gfx::Range(3), gfx::Range(), 1, 1, u"abefg", gfx::Range(2)},

      // Selection deletion.
      {gfx::Range(3, 4), gfx::Range(), 0, 0, u"abcefg", gfx::Range(3)},

      // Selection deletion with 1 char before.
      {gfx::Range(3, 4), gfx::Range(), 1, 0, u"abefg", gfx::Range(2)},

      // Selection deletion with 1 char after.
      {gfx::Range(3, 4), gfx::Range(), 0, 1, u"abcfg", gfx::Range(3)},

      // Selection deletion with 1 char for each before and after.
      {gfx::Range(3, 4), gfx::Range(), 1, 1, u"abfg", gfx::Range(2)},

      // With composition.
      {gfx::Range(2), gfx::Range(3, 4), 0, 0, u"abcefg", gfx::Range(2)},

      // With composition crossing the beginning boundary.
      {gfx::Range(1), gfx::Range(2, 5), 0, 2, u"afg", gfx::Range(1)},

      // With composition containing the selection.
      {gfx::Range(3, 4), gfx::Range(1, 6), 1, 1, u"ag", gfx::Range(1)},

      // With composition crossing the end boundary.
      {gfx::Range(6), gfx::Range(2, 5), 2, 0, u"abg", gfx::Range(2)},

      // With composition covered by selection.
      {gfx::Range(3, 4), gfx::Range(2, 5), 2, 2, u"ag", gfx::Range(1)},
  };

  for (const auto& test_case : kTestData) {
    SurroundingTextTracker tracker;
    ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
              tracker.Update(u"abcdefg", test_case.selection));
    if (!test_case.composition.is_empty())
      tracker.OnSetCompositionFromExistingText(test_case.composition);

    tracker.OnExtendSelectionAndDelete(test_case.before, test_case.after);
    EXPECT_EQ(test_case.expected_surrounding_text,
              tracker.predicted_state().surrounding_text);
    EXPECT_EQ(test_case.expected_selection,
              tracker.predicted_state().selection);
    EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  }
}

}  // namespace ui

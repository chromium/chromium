// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/surrounding_text_tracker.h"

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/composition_text.h"

namespace ui {

TEST(SurroundingTextTracker, StateGetSurroundingTextRange) {
  SurroundingTextTracker::State state{u"abcde", /*utf16_offset=*/10,
                                      /*selection=*/gfx::Range(),
                                      /*composition=*/gfx::Range()};

  EXPECT_EQ(gfx::Range(10, 15), state.GetSurroundingTextRange());
}

TEST(SurroundingTextTracker, StateGetCompositionText) {
  {
    SurroundingTextTracker::State state{u"abcde", /*utf16_offset=*/10,
                                        /*selection=*/gfx::Range(),
                                        /*composition=*/gfx::Range()};

    // Empty composition range is valid. Empty composition is expected.
    EXPECT_EQ(std::u16string_view(), state.GetCompositionText());
  }

  {
    SurroundingTextTracker::State state{u"abcde", /*utf16_offset=*/10,
                                        /*selection=*/gfx::Range(),
                                        /*composition=*/gfx::Range(11, 13)};

    EXPECT_EQ(std::u16string_view(u"bc"), state.GetCompositionText());
  }

  {
    SurroundingTextTracker::State state{u"abcde", /*utf16_offset=*/10,
                                        /*selection=*/gfx::Range(),
                                        /*composition=*/gfx::Range(1, 3)};

    // Out of the range case.
    EXPECT_EQ(std::nullopt, state.GetCompositionText());
  }

  {
    SurroundingTextTracker::State state{u"abcde", /*utf16_offset=*/10,
                                        /*selection=*/gfx::Range(),
                                        /*composition=*/gfx::Range(8, 12)};

    // Overlapping but not fully covered case.
    EXPECT_EQ(std::nullopt, state.GetCompositionText());
  }
}

TEST(SurroundingTextTracker, SetCompositionText) {
  SurroundingTextTracker tracker;

  ui::CompositionText composition;
  composition.text = u"abc";
  composition.selection = gfx::Range(3);  // at the end.

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"abc", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(3), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(0, 3), tracker.predicted_state().composition);

  composition.text = u"xyzw";
  composition.selection = gfx::Range(4);  // at the end.

  tracker.OnSetCompositionText(composition);
  EXPECT_EQ(u"xyzw", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(4), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(0, 4), tracker.predicted_state().composition);
}

TEST(SurroundingTextTracker, SetCompositionTextWithExistingText) {
  SurroundingTextTracker tracker;
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 0u,
                           gfx::Range(3)));  // Set cursor between c and d.

  ui::CompositionText composition;
  composition.text = u"xyz";
  composition.selection = gfx::Range(3);  // at the end.

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(6), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(3, 6), tracker.predicted_state().composition);

  composition.text = u"pqrst";
  composition.selection = gfx::Range(0);  // at beginning.

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"abcpqrstdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(3), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(3, 8), tracker.predicted_state().composition);

  // If there's selection, that will be replaced by composition.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 0u, gfx::Range(2, 5)));  // Select "cde".

  composition.text = u"xyz";
  composition.selection = gfx::Range(3);  // at the end.

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"abxyzfg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(5), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(2, 5), tracker.predicted_state().composition);

  // Simple check with offset.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u,
                           gfx::Range(8)));  // Set cursor between c and d.

  composition.text = u"xyz";
  composition.selection = gfx::Range(3);  // at the end.

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(5u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(11), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(8, 11), tracker.predicted_state().composition);

  // Selection is before offset.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u,
                           gfx::Range(3)));  // Set cursor before offset.

  composition.text = u"xyz";
  composition.selection = gfx::Range(3);  // at the end

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"xyz", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(3u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(6), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(3, 6), tracker.predicted_state().composition);

  // Selection ends at the beginning of surrounding text.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u, gfx::Range(3, 5)));

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"xyzabcdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(3u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(6), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(3, 6), tracker.predicted_state().composition);

  // Selection overlaps with the surrounding text.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u, gfx::Range(4, 6)));

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"xyzbcdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(4u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(7), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(4, 7), tracker.predicted_state().composition);

  // Selection covers whole surrounding text.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u, gfx::Range(4, 13)));

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"xyz", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(4u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(7), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(4, 7), tracker.predicted_state().composition);

  // Selection begins within the surrounding text, but overflows it.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u, gfx::Range(11, 13)));

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"abcdefxyz", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(5u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(14), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(11, 14), tracker.predicted_state().composition);

  // Selection begins at the end of the surrounding text.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u, gfx::Range(12, 13)));

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"abcdefgxyz", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(5u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(15), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(12, 15), tracker.predicted_state().composition);

  // Selection begins after the known surrounding text.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u, gfx::Range(13)));

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"xyz", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(13u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(16), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(13, 16), tracker.predicted_state().composition);
}

TEST(SurroundingTextTracker, SetCompositionFromExistingText) {
  SurroundingTextTracker tracker;
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 0u,
                           gfx::Range(3)));  // Set cursor between c and d.
  tracker.OnSetCompositionFromExistingText(gfx::Range(3, 5));
  EXPECT_EQ(u"abcdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(3), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(3, 5), tracker.predicted_state().composition);

  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u,
                           gfx::Range(8)));  // Set cursor between c and d.

  tracker.OnSetCompositionFromExistingText(gfx::Range(8, 10));
  EXPECT_EQ(u"abcdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(5u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(8), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(8, 10), tracker.predicted_state().composition);
}

TEST(SurroundingTextTracker, ConfirmCompositionText) {
  SurroundingTextTracker tracker;
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 0u,
                           gfx::Range(3)));  // Set cursor between c and d.

  ui::CompositionText composition;
  composition.text = u"xyz";
  composition.selection = gfx::Range(1);  // between x and y.

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(4), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(3, 6), tracker.predicted_state().composition);

  tracker.OnConfirmCompositionText(/*keep_selection=*/false);

  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(6), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());

  // Nothing happens if no composition exists.
  tracker.OnConfirmCompositionText(/*keep_selection=*/false);
  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(6), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());

  // Check with offset.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u,
                           gfx::Range(8)));  // Set cursor between c and d.
  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(5u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(9), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(8, 11), tracker.predicted_state().composition);

  tracker.OnConfirmCompositionText(/*keep_selection=*/false);

  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(5u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(11), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
}

TEST(SurroundingTextTracker, ConfirmCompositionTextWithKeepSelection) {
  SurroundingTextTracker tracker;
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 0u,
                           gfx::Range(3)));  // Set cursor between c and d.

  ui::CompositionText composition;
  composition.text = u"xyz";
  composition.selection = gfx::Range(1);  // between x and y.

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(4), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(3, 6), tracker.predicted_state().composition);

  tracker.OnConfirmCompositionText(/*keep_selection=*/true);

  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(4), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());

  // Nothing happens if no composition exists.
  tracker.OnConfirmCompositionText(/*keep_selection=*/true);
  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(4), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
}

TEST(SurroundingTextTracker, ClearCompositionText) {
  SurroundingTextTracker tracker;
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 0u,
                           gfx::Range(3)));  // Set cursor between c and d.

  ui::CompositionText composition;
  composition.text = u"xyz";
  composition.selection = gfx::Range(1);  // between x and y.

  tracker.OnSetCompositionText(composition);

  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(4), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(3, 6), tracker.predicted_state().composition);

  tracker.OnClearCompositionText();

  EXPECT_EQ(u"abcdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(3), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());

  // Set "cd" as composition text.
  tracker.OnSetCompositionFromExistingText(gfx::Range(2, 4));
  EXPECT_EQ(u"abcdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(3), tracker.predicted_state().selection);
  EXPECT_EQ(gfx::Range(2, 4), tracker.predicted_state().composition);

  // Then clear it again.
  tracker.OnClearCompositionText();

  EXPECT_EQ(u"abefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(2), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());

  // Nothing should happen if there's no composition.
  tracker.OnClearCompositionText();
  EXPECT_EQ(u"abefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(2), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());

  // With offset.
  // Set composition before the surrounding text.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u,
                           gfx::Range(8)));  // Set cursor between c and d.
  tracker.OnSetCompositionFromExistingText(gfx::Range(2, 4));
  tracker.OnClearCompositionText();

  EXPECT_EQ(u"abcdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(3u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(2), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());

  // Composition overlaps with the surrounding text.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u,
                           gfx::Range(8)));  // Set cursor between c and d.
  tracker.OnSetCompositionFromExistingText(gfx::Range(4, 6));
  tracker.OnClearCompositionText();

  EXPECT_EQ(u"bcdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(4u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(4), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());

  // Composition covers the whole surrounding text.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u,
                           gfx::Range(8)));  // Set cursor between c and d.
  tracker.OnSetCompositionFromExistingText(gfx::Range(4, 13));
  tracker.OnClearCompositionText();

  EXPECT_EQ(u"", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(4u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(4), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());

  // Composition overlaps the trailing part of surrounding text.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u,
                           gfx::Range(8)));  // Set cursor between c and d.
  tracker.OnSetCompositionFromExistingText(gfx::Range(11, 13));
  tracker.OnClearCompositionText();

  EXPECT_EQ(u"abcdef", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(5u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(11), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());

  // Composition is set after the surrounding text.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u,
                           gfx::Range(8)));  // Set cursor between c and d.
  tracker.OnSetCompositionFromExistingText(gfx::Range(13, 15));
  tracker.OnClearCompositionText();

  EXPECT_EQ(u"abcdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(5u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(13), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
}

TEST(SurroundingTextTracker, InsertText) {
  SurroundingTextTracker tracker;

  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 0u,
                           gfx::Range(3)));  // Set cursor between c and d.

  tracker.OnInsertText(
      u"xyz", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(6), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  EXPECT_EQ(SurroundingTextTracker::UpdateResult::kUpdated,
            tracker.Update(u"abcxyzdefg", 0u, gfx::Range(6)));

  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 0u,
                           gfx::Range(3)));  // Set cursor between c and d.

  tracker.OnInsertText(
      u"xyz", TextInputClient::InsertTextCursorBehavior::kMoveCursorBeforeText);
  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(3), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  EXPECT_EQ(SurroundingTextTracker::UpdateResult::kUpdated,
            tracker.Update(u"abcxyzdefg", 0u, gfx::Range(3)));

  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 0u,
                           gfx::Range(3, 4)));  // Set selection on "d".

  tracker.OnInsertText(
      u"xyz", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(u"abcxyzefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(6), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  EXPECT_EQ(SurroundingTextTracker::UpdateResult::kUpdated,
            tracker.Update(u"abcxyzefg", 0u, gfx::Range(6)));

  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 0u,
                           gfx::Range(3, 4)));  // Set selection on "d".

  tracker.OnInsertText(
      u"xyz", TextInputClient::InsertTextCursorBehavior::kMoveCursorBeforeText);
  // 'd' should be replaced.
  EXPECT_EQ(u"abcxyzefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(0u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(3), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  EXPECT_EQ(SurroundingTextTracker::UpdateResult::kUpdated,
            tracker.Update(u"abcxyzefg", 0u, gfx::Range(3)));

  // With offset.
  tracker.Reset();
  ASSERT_EQ(
      SurroundingTextTracker::UpdateResult::kReset,
      tracker.Update(u"abcdefg", 5u, gfx::Range(8)));  // Set cursor after "c"

  tracker.OnInsertText(
      u"xyz", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(u"abcxyzdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(5u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(11), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  EXPECT_EQ(SurroundingTextTracker::UpdateResult::kUpdated,
            tracker.Update(u"abcxyzdefg", 5u, gfx::Range(11)));

  // Selection is before offset.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u, gfx::Range(3, 4)));

  tracker.OnInsertText(
      u"xyz", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(u"xyz", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(3u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(6), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  EXPECT_EQ(SurroundingTextTracker::UpdateResult::kUpdated,
            tracker.Update(u"xyz", 3u, gfx::Range(6)));

  // Selection ends at the beginning of the surrounding text.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u, gfx::Range(3, 5)));

  tracker.OnInsertText(
      u"xyz", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(u"xyzabcdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(3u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(6), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  EXPECT_EQ(SurroundingTextTracker::UpdateResult::kUpdated,
            tracker.Update(u"xyzabcdefg", 3u, gfx::Range(6)));

  // Selection overlaps the leading parts of surrounding text.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u, gfx::Range(4, 6)));

  tracker.OnInsertText(
      u"xyz", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(u"xyzbcdefg", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(4u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(7), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  EXPECT_EQ(SurroundingTextTracker::UpdateResult::kUpdated,
            tracker.Update(u"xyzbcdefg", 4u, gfx::Range(7)));

  // Selection covers the whole surrounding text.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u, gfx::Range(4, 13)));

  tracker.OnInsertText(
      u"xyz", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(u"xyz", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(4u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(7), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  EXPECT_EQ(SurroundingTextTracker::UpdateResult::kUpdated,
            tracker.Update(u"xyz", 4u, gfx::Range(7)));

  // Selection overlaps the trailing part of surrounding text.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u, gfx::Range(11, 13)));

  tracker.OnInsertText(
      u"xyz", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(u"abcdefxyz", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(5u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(14), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  EXPECT_EQ(SurroundingTextTracker::UpdateResult::kUpdated,
            tracker.Update(u"abcdefxyz", 5u, gfx::Range(14)));

  // Selection starts wat the end of the surrounding text.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u, gfx::Range(12, 13)));

  tracker.OnInsertText(
      u"xyz", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(u"abcdefgxyz", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(5u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(15), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  EXPECT_EQ(SurroundingTextTracker::UpdateResult::kUpdated,
            tracker.Update(u"abcdefgxyz", 5u, gfx::Range(15)));

  // Selection is after the surrounding text.
  tracker.Reset();
  ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
            tracker.Update(u"abcdefg", 5u, gfx::Range(13, 14)));

  tracker.OnInsertText(
      u"xyz", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(u"xyz", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(13u, tracker.predicted_state().utf16_offset);
  EXPECT_EQ(gfx::Range(16), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  EXPECT_EQ(SurroundingTextTracker::UpdateResult::kUpdated,
            tracker.Update(u"xyz", 13u, gfx::Range(16)));
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
              tracker.Update(u"abcdefg", 0u, test_case.selection));

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
    size_t utf16_offset;
    gfx::Range selection;
    gfx::Range composition;
    size_t before;
    size_t after;
    const char16_t* expected_surrounding_text;
    size_t expected_utf16_offset;
    gfx::Range expected_selection;
  } kTestData[] = {
      // null deletion.
      {0u, gfx::Range(3), gfx::Range(), 0, 0, u"abcdefg", 0u, gfx::Range(3)},

      // Remove 1 char before the cursor.
      {0u, gfx::Range(3), gfx::Range(), 1, 0, u"abdefg", 0u, gfx::Range(2)},

      // Remove 1 char after the cursor.
      {0u, gfx::Range(3), gfx::Range(), 0, 1, u"abcefg", 0u, gfx::Range(3)},

      // Remove 1 char for each before and after the cursor.
      {0u, gfx::Range(3), gfx::Range(), 1, 1, u"abefg", 0u, gfx::Range(2)},

      // Selection deletion.
      {0u, gfx::Range(3, 4), gfx::Range(), 0, 0, u"abcefg", 0u, gfx::Range(3)},

      // Selection deletion with 1 char before.
      {0u, gfx::Range(3, 4), gfx::Range(), 1, 0, u"abefg", 0u, gfx::Range(2)},

      // Selection deletion with 1 char after.
      {0u, gfx::Range(3, 4), gfx::Range(), 0, 1, u"abcfg", 0u, gfx::Range(3)},

      // Selection deletion with 1 char for each before and after.
      {0u, gfx::Range(3, 4), gfx::Range(), 1, 1, u"abfg", 0u, gfx::Range(2)},

      // With composition.
      {0u, gfx::Range(2), gfx::Range(3, 4), 0, 0, u"abcefg", 0u, gfx::Range(2)},

      // With composition crossing the beginning boundary.
      {0u, gfx::Range(1), gfx::Range(2, 5), 0, 2, u"afg", 0u, gfx::Range(1)},

      // With composition containing the selection.
      {0u, gfx::Range(3, 4), gfx::Range(1, 6), 1, 1, u"ag", 0u, gfx::Range(1)},

      // With composition crossing the end boundary.
      {0u, gfx::Range(6), gfx::Range(2, 5), 2, 0, u"abg", 0u, gfx::Range(2)},

      // With composition covered by selection.
      {0u, gfx::Range(3, 4), gfx::Range(2, 5), 2, 2, u"ag", 0u, gfx::Range(1)},

      // With offset.
      {5u, gfx::Range(8), gfx::Range(), 1, 2, u"abfg", 5u, gfx::Range(7)},
      {5u, gfx::Range(3), gfx::Range(), 1, 0, u"abcdefg", 4u, gfx::Range(2)},
      {5u, gfx::Range(3), gfx::Range(), 0, 1, u"abcdefg", 4u, gfx::Range(3)},
      {5u, gfx::Range(4), gfx::Range(), 0, 2, u"bcdefg", 4u, gfx::Range(4)},
      {5u, gfx::Range(13), gfx::Range(), 1, 0, u"abcdefg", 5u, gfx::Range(12)},
      {5u, gfx::Range(13), gfx::Range(), 0, 1, u"abcdefg", 5u, gfx::Range(13)},
      {5u, gfx::Range(13), gfx::Range(), 2, 0, u"abcdef", 5u, gfx::Range(11)},
  };

  for (const auto& test_case : kTestData) {
    SurroundingTextTracker tracker;
    ASSERT_EQ(SurroundingTextTracker::UpdateResult::kReset,
              tracker.Update(u"abcdefg", test_case.utf16_offset,
                             test_case.selection));
    if (!test_case.composition.is_empty())
      tracker.OnSetCompositionFromExistingText(test_case.composition);

    tracker.OnExtendSelectionAndDelete(test_case.before, test_case.after);
    EXPECT_EQ(test_case.expected_surrounding_text,
              tracker.predicted_state().surrounding_text);
    EXPECT_EQ(test_case.expected_utf16_offset,
              tracker.predicted_state().utf16_offset);
    EXPECT_EQ(test_case.expected_selection,
              tracker.predicted_state().selection);
    EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
  }
}

TEST(SurroundingTextTracker, CancelCompositionResetsCompositionOnly) {
  SurroundingTextTracker tracker;
  ui::CompositionText composition;
  composition.text = u"abc";
  composition.selection = gfx::Range(3);  // at the end.
  tracker.OnSetCompositionText(composition);

  tracker.CancelComposition();

  EXPECT_EQ(u"abc", tracker.predicted_state().surrounding_text);
  EXPECT_EQ(gfx::Range(3), tracker.predicted_state().selection);
  EXPECT_TRUE(tracker.predicted_state().composition.is_empty());
}

}  // namespace ui

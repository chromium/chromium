// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// TODO(nona): Add more tests.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/ime/candidate_window.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {

const size_t kSampleCandidateSize = 3;
const char* kSampleCandidate[] = {
  "Sample Candidate 1",
  "Sample Candidate 2",
  "Sample Candidate 3",
};
const char* kSampleDescriptionTitle[] = {
  "Sample Description Title 1",
  "Sample Description Title 2",
  "Sample Description Title 3",
};
const char* kSampleDescriptionBody[] = {
  "Sample Description Body 1",
  "Sample Description Body 2",
  "Sample Description Body 3",
};

}

TEST(CandidateWindow, IsEqualTest) {
  CandidateWindow cw1;
  CandidateWindow cw2;

  const std::u16string kSampleString1 = u"Sample 1";
  const std::u16string kSampleString2 = u"Sample 2";

  EXPECT_TRUE(cw1.IsEqual(cw2));
  EXPECT_TRUE(cw2.IsEqual(cw1));

  cw1.set_page_size(1);
  cw2.set_page_size(2);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw2.set_page_size(1);

  cw1.set_cursor_position(1);
  cw2.set_cursor_position(2);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw2.set_cursor_position(1);

  cw1.set_is_cursor_visible(true);
  cw2.set_is_cursor_visible(false);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw2.set_is_cursor_visible(true);

  cw1.set_orientation(CandidateWindow::HORIZONTAL);
  cw2.set_orientation(CandidateWindow::VERTICAL);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw2.set_orientation(CandidateWindow::HORIZONTAL);

  cw1.set_show_window_at_composition(true);
  cw2.set_show_window_at_composition(false);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw2.set_show_window_at_composition(true);

  // Check equality for candidates member variable.
  CandidateWindow::Entry entry1;
  CandidateWindow::Entry entry2;

  cw1.mutable_candidates()->push_back(entry1);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw2.mutable_candidates()->push_back(entry2);
  EXPECT_TRUE(cw1.IsEqual(cw2));
  EXPECT_TRUE(cw2.IsEqual(cw1));

  entry1.value = kSampleString1;
  entry2.value = kSampleString2;
  cw1.mutable_candidates()->push_back(entry1);
  cw2.mutable_candidates()->push_back(entry2);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw1.mutable_candidates()->clear();
  cw2.mutable_candidates()->clear();

  entry1.label = kSampleString1;
  entry2.label = kSampleString2;
  cw1.mutable_candidates()->push_back(entry1);
  cw2.mutable_candidates()->push_back(entry2);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw1.mutable_candidates()->clear();
  cw2.mutable_candidates()->clear();

  entry1.annotation = kSampleString1;
  entry2.annotation = kSampleString2;
  cw1.mutable_candidates()->push_back(entry1);
  cw2.mutable_candidates()->push_back(entry2);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw1.mutable_candidates()->clear();
  cw2.mutable_candidates()->clear();

  entry1.description_title = kSampleString1;
  entry2.description_title = kSampleString2;
  cw1.mutable_candidates()->push_back(entry1);
  cw2.mutable_candidates()->push_back(entry2);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw1.mutable_candidates()->clear();
  cw2.mutable_candidates()->clear();

  entry1.description_body = kSampleString1;
  entry2.description_body = kSampleString2;
  cw1.mutable_candidates()->push_back(entry1);
  cw2.mutable_candidates()->push_back(entry2);
  EXPECT_FALSE(cw1.IsEqual(cw2));
  EXPECT_FALSE(cw2.IsEqual(cw1));
  cw1.mutable_candidates()->clear();
  cw2.mutable_candidates()->clear();
}

TEST(CandidateWindow, CopyFromTest) {
  CandidateWindow cw1;
  CandidateWindow cw2;

  const std::u16string kSampleString = u"Sample";

  cw1.set_page_size(1);
  cw1.set_cursor_position(2);
  cw1.set_is_cursor_visible(false);
  cw1.set_orientation(CandidateWindow::HORIZONTAL);
  cw1.set_show_window_at_composition(false);

  CandidateWindow::Entry entry;
  entry.value = kSampleString;
  entry.label = kSampleString;
  entry.annotation = kSampleString;
  entry.description_title = kSampleString;
  entry.description_body = kSampleString;
  cw1.mutable_candidates()->push_back(entry);

  cw2.CopyFrom(cw1);
  EXPECT_TRUE(cw1.IsEqual(cw2));
}

TEST(CandidateWindow, GetInfolistEntries_DenseCase) {
  CandidateWindow candidate_window;
  candidate_window.set_page_size(10);
  for (size_t i = 0; i < kSampleCandidateSize; ++i) {
    CandidateWindow::Entry entry;
    entry.value = base::UTF8ToUTF16(kSampleCandidate[i]);
    entry.description_title = base::UTF8ToUTF16(kSampleDescriptionTitle[i]);
    entry.description_body = base::UTF8ToUTF16(kSampleDescriptionBody[i]);
    candidate_window.mutable_candidates()->push_back(entry);
  }
  candidate_window.set_cursor_position(1);

  std::vector<InfolistEntry> infolist_entries;
  bool has_highlighted = false;

  candidate_window.GetInfolistEntries(&infolist_entries, &has_highlighted);

  EXPECT_EQ(kSampleCandidateSize, infolist_entries.size());
  EXPECT_TRUE(has_highlighted);
  EXPECT_TRUE(infolist_entries[1].highlighted);
}

TEST(CandidateWindow, GetInfolistEntries_SparseCase) {
  CandidateWindow candidate_window;
  candidate_window.set_page_size(10);
  for (size_t i = 0; i < kSampleCandidateSize; ++i) {
    CandidateWindow::Entry entry;
    entry.value = base::UTF8ToUTF16(kSampleCandidate[i]);
    candidate_window.mutable_candidates()->push_back(entry);
  }

  std::vector<CandidateWindow::Entry>* candidates =
      candidate_window.mutable_candidates();
  (*candidates)[2].description_title =
      base::UTF8ToUTF16(kSampleDescriptionTitle[2]);
  (*candidates)[2].description_body =
      base::UTF8ToUTF16(kSampleDescriptionBody[2]);

  candidate_window.set_cursor_position(2);

  std::vector<InfolistEntry> infolist_entries;
  bool has_highlighted = false;

  candidate_window.GetInfolistEntries(&infolist_entries, &has_highlighted);

  // Infolist entries skips empty descriptions, so expected entry size is 1.
  EXPECT_EQ(1UL, infolist_entries.size());
  EXPECT_TRUE(has_highlighted);
  EXPECT_TRUE(infolist_entries[0].highlighted);
}

TEST(CandidateWindow, GetInfolistEntries_SparseNoSelectionCase) {
  CandidateWindow candidate_window;
  candidate_window.set_page_size(10);

  for (size_t i = 0; i < kSampleCandidateSize; ++i) {
    CandidateWindow::Entry entry;
    entry.value = base::UTF8ToUTF16(kSampleCandidate[i]);
    candidate_window.mutable_candidates()->push_back(entry);
  }

  std::vector<CandidateWindow::Entry>* candidates =
      candidate_window.mutable_candidates();
  (*candidates)[2].description_title =
      base::UTF8ToUTF16(kSampleDescriptionTitle[2]);
  (*candidates)[2].description_body =
      base::UTF8ToUTF16(kSampleDescriptionBody[2]);

  candidate_window.set_cursor_position(0);

  std::vector<InfolistEntry> infolist_entries;
  bool has_highlighted;

  candidate_window.GetInfolistEntries(&infolist_entries, &has_highlighted);

  // Infolist entries skips empty descriptions, so expected entry size is 1 and
  // no highlighted entries.
  EXPECT_EQ(1UL, infolist_entries.size());
  EXPECT_FALSE(has_highlighted);
  EXPECT_FALSE(infolist_entries[0].highlighted);
}

TEST(CandidateWindow, GetInfolistEntries_NoInfolistCase) {
  CandidateWindow candidate_window;
  candidate_window.set_page_size(10);

  for (size_t i = 0; i < kSampleCandidateSize; ++i) {
    CandidateWindow::Entry entry;
    entry.value = base::UTF8ToUTF16(kSampleCandidate[i]);
    candidate_window.mutable_candidates()->push_back(entry);
  }
  candidate_window.set_cursor_position(1);

  std::vector<InfolistEntry> infolist_entries;
  bool has_highlighted = false;

  candidate_window.GetInfolistEntries(&infolist_entries, &has_highlighted);

  EXPECT_TRUE(infolist_entries.empty());
  EXPECT_FALSE(has_highlighted);
}

}  // namespace ui

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/candidate_window.h"

#include <stddef.h>

#include <string>
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace ui {

namespace {
// The default entry number of a page in CandidateWindow.
const int kDefaultPageSize = 9;
}  // namespace

CandidateWindow::CandidateWindow()
    : property_(new CandidateWindowProperty) {
}

CandidateWindow::~CandidateWindow() {
}

bool CandidateWindow::IsEqual(const CandidateWindow& cw) const {
  if (page_size() != cw.page_size() ||
      cursor_position() != cw.cursor_position() ||
      is_cursor_visible() != cw.is_cursor_visible() ||
      orientation() != cw.orientation() ||
      show_window_at_composition() != cw.show_window_at_composition() ||
      is_auxiliary_text_visible() != cw.is_auxiliary_text_visible() ||
      auxiliary_text() != cw.auxiliary_text() ||
      candidates_.size() != cw.candidates_.size() ||
      is_user_selecting() != cw.is_user_selecting()) {
    return false;
  }

  for (size_t i = 0; i < candidates_.size(); ++i) {
    const Entry& left = candidates_[i];
    const Entry& right = cw.candidates_[i];
    if (left.value != right.value ||
        left.label != right.label ||
        left.annotation != right.annotation ||
        left.description_title != right.description_title ||
        left.description_body != right.description_body)
      return false;
  }
  return true;
}

void CandidateWindow::CopyFrom(const CandidateWindow& cw) {
  SetProperty(cw.GetProperty());
  candidates_.clear();
  candidates_ = cw.candidates_;
}


void CandidateWindow::GetInfolistEntries(
    std::vector<ui::InfolistEntry>* infolist_entries,
    bool* has_highlighted) const {
  DCHECK(infolist_entries);
  DCHECK(has_highlighted);
  infolist_entries->clear();
  *has_highlighted = false;

  const size_t cursor_index_in_page = cursor_position() % page_size();

  for (size_t i = 0; i < candidates().size(); ++i) {
    const CandidateWindow::Entry& candidate_entry = candidates()[i];
    if (candidate_entry.description_title.empty() &&
        candidate_entry.description_body.empty())
      continue;

    InfolistEntry entry(candidate_entry.description_title,
                        candidate_entry.description_body);
    if (i == cursor_index_in_page) {
      entry.highlighted = true;
      *has_highlighted = true;
    }
    infolist_entries->push_back(entry);
  }
}

// When the default values are changed, please modify
// InputMethodEngineInterface::CandidateWindowProperty too.
CandidateWindow::CandidateWindowProperty::CandidateWindowProperty()
    : page_size(kDefaultPageSize),
      cursor_position(0),
      is_cursor_visible(true),
      is_vertical(false),
      show_window_at_composition(false),
      is_auxiliary_text_visible(false),
      current_candidate_index(-1),
      total_candidates(0),
      is_user_selecting(false) {}

CandidateWindow::CandidateWindowProperty::~CandidateWindowProperty() {
}

CandidateWindow::Entry::Entry() {
}

CandidateWindow::Entry::Entry(const Entry& other) = default;

CandidateWindow::Entry::~Entry() {
}

}  // namespace ui

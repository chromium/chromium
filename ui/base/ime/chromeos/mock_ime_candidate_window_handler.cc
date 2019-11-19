// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/mock_ime_candidate_window_handler.h"

namespace chromeos {

MockIMECandidateWindowHandler::MockIMECandidateWindowHandler()
    : set_cursor_bounds_call_count_(0),
      update_lookup_table_call_count_(0) {
}

MockIMECandidateWindowHandler::~MockIMECandidateWindowHandler() = default;

void MockIMECandidateWindowHandler::UpdateLookupTable(
    const ui::CandidateWindow& table,
    bool visible) {
  ++update_lookup_table_call_count_;
  last_update_lookup_table_arg_.lookup_table.CopyFrom(table);
  last_update_lookup_table_arg_.is_visible = visible;
}

void MockIMECandidateWindowHandler::UpdatePreeditText(
    const base::string16& text,
    uint32_t cursor_pos,
    bool visible) {}

void MockIMECandidateWindowHandler::SetCursorBounds(
    const gfx::Rect& cursor_bounds,
    const gfx::Rect& composition_head) {
  ++set_cursor_bounds_call_count_;
}

gfx::Rect MockIMECandidateWindowHandler::GetCursorBounds() const {
  return gfx::Rect(1, 1, 1, 1);
}

void MockIMECandidateWindowHandler::Reset() {
  set_cursor_bounds_call_count_ = 0;
  update_lookup_table_call_count_ = 0;
}

}  // namespace chromeos

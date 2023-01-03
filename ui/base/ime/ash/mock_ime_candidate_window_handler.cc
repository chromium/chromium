// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/mock_ime_candidate_window_handler.h"

namespace ash {

MockIMECandidateWindowHandler::MockIMECandidateWindowHandler()
    : set_cursor_and_composition_bounds_call_count_(0),
      update_lookup_table_call_count_(0) {}

MockIMECandidateWindowHandler::~MockIMECandidateWindowHandler() = default;

void MockIMECandidateWindowHandler::HideLookupTable() {
  ++update_lookup_table_call_count_;
  last_update_lookup_table_arg_.is_visible = false;
}

void MockIMECandidateWindowHandler::UpdateLookupTable(
    const ui::CandidateWindow& table) {
  ++update_lookup_table_call_count_;
  last_update_lookup_table_arg_.lookup_table.CopyFrom(table);
  last_update_lookup_table_arg_.is_visible = true;
}

void MockIMECandidateWindowHandler::UpdatePreeditText(
    const std::u16string& text,
    uint32_t cursor_pos,
    bool visible) {}

void MockIMECandidateWindowHandler::SetCursorAndCompositionBounds(
    const gfx::Rect& cursor_bounds,
    const gfx::Rect& composition_bounds) {
  ++set_cursor_and_composition_bounds_call_count_;
}

gfx::Rect MockIMECandidateWindowHandler::GetCursorBounds() const {
  return gfx::Rect(1, 1, 1, 1);
}

void MockIMECandidateWindowHandler::Reset() {
  set_cursor_and_composition_bounds_call_count_ = 0;
  update_lookup_table_call_count_ = 0;
}

}  // namespace ash

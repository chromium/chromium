// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_MOCK_IME_CANDIDATE_WINDOW_HANDLER_H_
#define UI_BASE_IME_ASH_MOCK_IME_CANDIDATE_WINDOW_HANDLER_H_

#include <stdint.h>

#include "base/component_export.h"
#include "ui/base/ime/ash/ime_candidate_window_handler_interface.h"
#include "ui/base/ime/candidate_window.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class COMPONENT_EXPORT(UI_BASE_IME_ASH) MockIMECandidateWindowHandler
    : public IMECandidateWindowHandlerInterface {
 public:
  struct UpdateLookupTableArg {
    ui::CandidateWindow lookup_table;
    bool is_visible;
  };

  struct UpdateAuxiliaryTextArg {
    std::string text;
    bool is_visible;
  };

  MockIMECandidateWindowHandler();
  ~MockIMECandidateWindowHandler() override;

  // IMECandidateWindowHandlerInterface override.
  void HideLookupTable() override;
  void UpdateLookupTable(const ui::CandidateWindow& candidate_window) override;
  void UpdatePreeditText(const std::u16string& text,
                         uint32_t cursor_pos,
                         bool visible) override;
  void SetCursorAndCompositionBounds(
      const gfx::Rect& cursor_bounds,
      const gfx::Rect& composition_bounds) override;
  gfx::Rect GetCursorBounds() const override;

  int set_cursor_and_composition_bounds_call_count() const {
    return set_cursor_and_composition_bounds_call_count_;
  }

  int update_lookup_table_call_count() const {
    return update_lookup_table_call_count_;
  }

  const UpdateLookupTableArg& last_update_lookup_table_arg() {
    return last_update_lookup_table_arg_;
  }
  // Resets all call count.
  void Reset();

 private:
  int set_cursor_and_composition_bounds_call_count_;
  int update_lookup_table_call_count_;
  UpdateLookupTableArg last_update_lookup_table_arg_;
};

}  // namespace ash

#endif  // UI_BASE_IME_ASH_MOCK_IME_CANDIDATE_WINDOW_HANDLER_H_

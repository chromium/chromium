// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_TEST_API_H_
#define UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_TEST_API_H_

#include "base/memory/raw_ptr.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/textfield/textfield.h"

namespace views {

// Helper class to access internal state of Textfield in tests.
class TextfieldTestApi {
 public:
  explicit TextfieldTestApi(Textfield* textfield);
  TextfieldTestApi(const TextfieldTestApi&) = delete;
  TextfieldTestApi& operator=(const TextfieldTestApi&) = delete;
  ~TextfieldTestApi() = default;

  void UpdateContextMenu();

  gfx::RenderText* GetRenderText() const;

  void CreateTouchSelectionControllerAndNotifyIt();

  void ResetTouchSelectionController();

  TextfieldModel* model() const { return textfield_->model_.get(); }

  void ExecuteTextEditCommand(ui::TextEditCommand command) {
    textfield_->ExecuteTextEditCommand(command);
  }

  ui::MenuModel* context_menu_contents() const {
    return textfield_->context_menu_contents_.get();
  }

  TouchSelectionController* touch_selection_controller() const {
    return textfield_->touch_selection_controller_.get();
  }

  ui::TextEditCommand scheduled_text_edit_command() const {
    return textfield_->scheduled_text_edit_command_;
  }

  bool IsCursorBlinkTimerRunning() const {
    return textfield_->cursor_blink_timer_.IsRunning();
  }

  gfx::Rect GetCursorViewRect() { return textfield_->cursor_view_->bounds(); }
  void SetCursorViewRect(gfx::Rect bounds);

  bool IsCursorVisible() const {
    return textfield_->cursor_view_->GetVisible();
  }

  bool ShouldShowCursor() const;

  float CursorLayerOpacity() {
    return textfield_->cursor_view_->layer()->opacity();
  }

  void SetCursorLayerOpacity(float opacity) {
    textfield_->cursor_view_->layer()->SetOpacity(opacity);
  }

  void UpdateCursorVisibility() { textfield_->UpdateCursorVisibility(); }

  void FlashCursor() { textfield_->OnCursorBlinkTimerFired(); }

  int GetDisplayOffsetX() const;
  void SetDisplayOffsetX(int x) const;

 private:
  const raw_ptr<Textfield> textfield_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_TEST_API_H_

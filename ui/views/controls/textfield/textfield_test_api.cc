// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/textfield_test_api.h"

#include "ui/gfx/geometry/rect.h"

namespace views {

TextfieldTestApi::TextfieldTestApi(Textfield* textfield)
    : textfield_(textfield) {
  DCHECK(textfield);
}

void TextfieldTestApi::UpdateContextMenu() {
  textfield_->UpdateContextMenu();
}

gfx::RenderText* TextfieldTestApi::GetRenderText() const {
  return textfield_->GetRenderText();
}

void TextfieldTestApi::CreateTouchSelectionControllerAndNotifyIt() {
  textfield_->CreateTouchSelectionControllerAndNotifyIt();
}

void TextfieldTestApi::ResetTouchSelectionController() {
  textfield_->touch_selection_controller_.reset();
}

void TextfieldTestApi::SetCursorViewRect(gfx::Rect bounds) {
  textfield_->cursor_view_->SetBoundsRect(bounds);
}

bool TextfieldTestApi::ShouldShowCursor() const {
  return textfield_->ShouldShowCursor();
}

int TextfieldTestApi::GetDisplayOffsetX() const {
  return GetRenderText()->GetUpdatedDisplayOffset().x();
}

void TextfieldTestApi::SetDisplayOffsetX(int x) const {
  return GetRenderText()->SetDisplayOffset(x);
}

}  // namespace views

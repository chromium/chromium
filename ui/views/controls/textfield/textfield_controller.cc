// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/textfield_controller.h"

#include "base/functional/callback_helpers.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/events/event.h"

namespace views {

bool TextfieldController::HandleKeyEvent(Textfield* sender,
                                         const ui::KeyEvent& key_event) {
  return false;
}

bool TextfieldController::HandleMouseEvent(Textfield* sender,
                                           const ui::MouseEvent& mouse_event) {
  return false;
}

bool TextfieldController::HandleGestureEvent(
    Textfield* sender,
    const ui::GestureEvent& gesture_event) {
  return false;
}

ui::mojom::DragOperation TextfieldController::OnDrop(
    const ui::DropTargetEvent& event) {
  return ui::mojom::DragOperation::kNone;
}

views::View::DropCallback TextfieldController::CreateDropCallback(
    const ui::DropTargetEvent& event) {
  return base::NullCallback();
}

bool TextfieldController::HandleWriteTextToClipboard(
    ui::ClipboardBuffer clipboard_buffer,
    const std::u16string_view& text) {
  return false;
}

bool TextfieldController::AllowStartDragEvent(const std::u16string_view&) {
  return true;
}

}  // namespace views

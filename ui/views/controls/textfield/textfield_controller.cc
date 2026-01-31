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

bool TextfieldController::OnBeforeCutOrCopy(Textfield* sender,
                                            std::u16string* copy_contents) {
  // Default implementation does not intercept copy/cut. Controllers may
  // override this to supply copy/cut contents and return true to bypass default
  // clipboard write.
  return false;
}

bool TextfieldController::OnBeforePaste(Textfield* sender,
                                        std::u16string* paste_contents) {
  // Default implementation does not intercept paste. Controllers may override
  // this to supply paste contents and return true to bypass default clipboard
  // read.
  return false;
}

std::unique_ptr<ui::ScopedClipboardWriter>
TextfieldController::CreateClipboardWriter() {
  return std::make_unique<ui::ScopedClipboardWriter>(
      ui::ClipboardBuffer::kCopyPaste);
}

}  // namespace views

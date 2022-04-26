// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/linux/fake_input_method_context.h"

namespace ui {

FakeInputMethodContext::FakeInputMethodContext() {}

// Overriden from ui::LinuxInputMethodContext

bool FakeInputMethodContext::DispatchKeyEvent(
    const ui::KeyEvent& /* key_event */) {
  return false;
}

bool FakeInputMethodContext::IsPeekKeyEvent(const ui::KeyEvent& key_event) {
  return false;
}

void FakeInputMethodContext::Reset() {
}

void FakeInputMethodContext::Focus() {
}

void FakeInputMethodContext::Blur() {
}

void FakeInputMethodContext::SetCursorLocation(const gfx::Rect& rect) {
}

void FakeInputMethodContext::SetSurroundingText(
    const std::u16string& text,
    const gfx::Range& selection_range) {}

void FakeInputMethodContext::SetContentType(TextInputType type,
                                            TextInputMode mode,
                                            uint32_t flags,
                                            bool should_do_learning) {}

VirtualKeyboardController*
FakeInputMethodContext::GetVirtualKeyboardController() {
  return nullptr;
}

}  // namespace ui

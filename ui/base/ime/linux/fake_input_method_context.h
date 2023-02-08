// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_LINUX_FAKE_INPUT_METHOD_CONTEXT_H_
#define UI_BASE_IME_LINUX_FAKE_INPUT_METHOD_CONTEXT_H_

#include "base/component_export.h"
#include "ui/base/ime/linux/linux_input_method_context.h"

namespace ui {

// A fake implementation of LinuxInputMethodContext, which does nothing.
class COMPONENT_EXPORT(UI_BASE_IME_LINUX) FakeInputMethodContext
    : public LinuxInputMethodContext {
 public:
  FakeInputMethodContext();

  FakeInputMethodContext(const FakeInputMethodContext&) = delete;
  FakeInputMethodContext& operator=(const FakeInputMethodContext&) = delete;

  // Overriden from ui::LinuxInputMethodContext
  bool DispatchKeyEvent(const ui::KeyEvent& key_event) override;
  bool IsPeekKeyEvent(const ui::KeyEvent& key_event) override;
  void Reset() override;
  void UpdateFocus(bool has_client,
                   TextInputType old_type,
                   TextInputType new_type,
                   TextInputClient::FocusReason reason) override;
  void SetCursorLocation(const gfx::Rect& rect) override;
  void SetSurroundingText(const std::u16string& text,
                          const gfx::Range& selection_range) override;
  void SetContentType(TextInputType type,
                      TextInputMode mode,
                      uint32_t flags,
                      bool should_do_learning) override;
  void SetGrammarFragmentAtCursor(const ui::GrammarFragment& fragment) override;
  void SetAutocorrectInfo(const gfx::Range& autocorrect_range,
                          const gfx::Rect& autocorrect_bounds) override;
  VirtualKeyboardController* GetVirtualKeyboardController() override;
};

}  // namespace ui

#endif  // UI_BASE_IME_LINUX_FAKE_INPUT_METHOD_CONTEXT_H_

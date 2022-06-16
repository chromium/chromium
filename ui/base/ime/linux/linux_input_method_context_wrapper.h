// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_WRAPPER_H_
#define UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_WRAPPER_H_

#include <memory>

#include "base/component_export.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/base/ime/text_input_type.h"

namespace ui {

// Historically, LinuxInputMethodContext is instantiated for two variations,
// one for "simple" and the other for "non-simple".
// This is a class to help unify these into one. Specifically, we'd like to
// migrate each platform independently, and during the period, this class
// helps to fill the gap.
// TODO(crbug.com/1331183): Remove once all platforms are migrated.
class COMPONENT_EXPORT(UI_BASE_IME_LINUX) LinuxInputMethodContextWrapper
    : public LinuxInputMethodContext {
 public:
  LinuxInputMethodContextWrapper(
      std::unique_ptr<LinuxInputMethodContext> context,
      std::unique_ptr<LinuxInputMethodContext> simple_context);
  ~LinuxInputMethodContextWrapper() override;

  // LinuxInputMethodContext overrides:
  bool DispatchKeyEvent(const ui::KeyEvent& key_event) override;
  bool IsPeekKeyEvent(const ui::KeyEvent& key_event) override;
  void SetCursorLocation(const gfx::Rect& rect) override;
  void SetSurroundingText(const std::u16string& text,
                          const gfx::Range& selection_range) override;
  void SetContentType(TextInputType type,
                      TextInputMode mode,
                      uint32_t flags,
                      bool should_do_learning) override;
  void Reset() override;
  void UpdateFocus(bool has_client,
                   TextInputType old_type,
                   TextInputType new_type) override;
  VirtualKeyboardController* GetVirtualKeyboardController() override;

 private:
  LinuxInputMethodContext* GetBaseContext();

  std::unique_ptr<LinuxInputMethodContext> context_;
  std::unique_ptr<LinuxInputMethodContext> simple_context_;
  TextInputType current_type_ = TEXT_INPUT_TYPE_NONE;
};

}  // namespace ui

#endif  // UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_WRAPPER_H_

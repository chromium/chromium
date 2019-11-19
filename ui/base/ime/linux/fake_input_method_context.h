// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_LINUX_FAKE_INPUT_METHOD_CONTEXT_H_
#define UI_BASE_IME_LINUX_FAKE_INPUT_METHOD_CONTEXT_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/ime/linux/linux_input_method_context.h"

namespace ui {

// A fake implementation of LinuxInputMethodContext, which does nothing.
class COMPONENT_EXPORT(UI_BASE_IME_LINUX) FakeInputMethodContext
    : public LinuxInputMethodContext {
 public:
  FakeInputMethodContext();

  // Overriden from ui::LinuxInputMethodContext
  bool DispatchKeyEvent(const ui::KeyEvent& key_event) override;
  void Reset() override;
  void Focus() override;
  void Blur() override;
  void SetCursorLocation(const gfx::Rect& rect) override;
  void SetSurroundingText(const base::string16& text,
                          const gfx::Range& selection_range) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeInputMethodContext);
};

}  // namespace ui

#endif  // UI_BASE_IME_LINUX_FAKE_INPUT_METHOD_CONTEXT_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_MINIMAL_H_
#define UI_BASE_IME_INPUT_METHOD_MINIMAL_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/ime/input_method_base.h"

namespace ui {

// A minimal implementation of ui::InputMethod, which supports only the direct
// input without any compositions or conversions.
class COMPONENT_EXPORT(UI_BASE_IME) InputMethodMinimal
    : public InputMethodBase {
 public:
  explicit InputMethodMinimal(internal::InputMethodDelegate* delegate);
  ~InputMethodMinimal() override;

  // Overriden from InputMethod.
  ui::EventDispatchDetails DispatchKeyEvent(ui::KeyEvent* event) override;
  void OnCaretBoundsChanged(const TextInputClient* client) override;
  void CancelComposition(const TextInputClient* client) override;
  bool IsCandidatePopupOpen() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodMinimal);
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_MINIMAL_H_

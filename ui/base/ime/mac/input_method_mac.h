// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_MAC_INPUT_METHOD_MAC_H_
#define UI_BASE_IME_MAC_INPUT_METHOD_MAC_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/ime/input_method_base.h"

namespace ui {

// A ui::InputMethod implementation for Mac.
// On the Mac, key events don't pass through InputMethod.
// Instead, NSTextInputClient calls are bridged to the currently focused
// ui::TextInputClient object.
class COMPONENT_EXPORT(UI_BASE_IME_MAC) InputMethodMac
    : public InputMethodBase {
 public:
  explicit InputMethodMac(internal::InputMethodDelegate* delegate);
  ~InputMethodMac() override;

  // Overriden from InputMethod.
  ui::EventDispatchDetails DispatchKeyEvent(ui::KeyEvent* event) override;
  void OnCaretBoundsChanged(const TextInputClient* client) override;
  void CancelComposition(const TextInputClient* client) override;
  bool IsCandidatePopupOpen() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodMac);
};

}  // namespace ui

#endif  // UI_BASE_IME_MAC_INPUT_METHOD_MAC_H_

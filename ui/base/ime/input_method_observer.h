// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_OBSERVER_H_
#define UI_BASE_IME_INPUT_METHOD_OBSERVER_H_

#include "base/component_export.h"

namespace ui {

class InputMethod;
class TextInputClient;

class COMPONENT_EXPORT(UI_BASE_IME) InputMethodObserver {
 public:
  virtual ~InputMethodObserver() {}

  // Called when the top-level system window gets keyboard focus. Currently
  // only used by the mock input method for testing.
  virtual void OnFocus() = 0;

  // Called when the top-level system window loses keyboard focus. Currently
  // only used by the mock input method for testing.
  virtual void OnBlur() = 0;

  // Called whenever the caret bounds is changed for the input client.
  virtual void OnCaretBoundsChanged(const TextInputClient* client) = 0;

  // Called when either:
  //  - the TextInputClient is changed (e.g. by a change of focus)
  //  - the TextInputType of the TextInputClient changes
  virtual void OnTextInputStateChanged(const TextInputClient* client) = 0;

  // Called when the observed InputMethod is being destroyed.
  virtual void OnInputMethodDestroyed(const InputMethod* input_method) = 0;

  // Called when a user gesture should trigger showing the virtual keyboard
  // or alternate input view (e.g. handwriting palette). Used in ChromeOS.
  virtual void OnShowVirtualKeyboardIfEnabled() = 0;
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_OBSERVER_H_

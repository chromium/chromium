// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_DELEGATE_H_
#define UI_BASE_IME_INPUT_METHOD_DELEGATE_H_

#include "base/component_export.h"

namespace ui {

class KeyEvent;

struct EventDispatchDetails;

namespace internal {

// An interface implemented by the object that handles events sent back from an
// ui::InputMethod implementation.
class COMPONENT_EXPORT(UI_BASE_IME) InputMethodDelegate {
 public:
  virtual ~InputMethodDelegate() {}

  // Dispatch a key event already processed by the input method. Returns the
  // status of processing.
  virtual EventDispatchDetails DispatchKeyEventPostIME(KeyEvent* key_event) = 0;
};

}  // namespace internal
}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_DELEGATE_H_

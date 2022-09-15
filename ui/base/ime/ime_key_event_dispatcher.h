// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_IME_KEY_EVENT_DISPATCHER_H_
#define UI_BASE_IME_IME_KEY_EVENT_DISPATCHER_H_

#include "base/component_export.h"

namespace ui {

class KeyEvent;
struct EventDispatchDetails;

// An interface implemented by the object that handles key events sent from an
// ui::InputMethod.
class COMPONENT_EXPORT(UI_BASE_IME) ImeKeyEventDispatcher {
 public:
  // Dispatch a key event already processed by the input method. Returns the
  // status of processing.
  virtual EventDispatchDetails DispatchKeyEventPostIME(KeyEvent* key_event) = 0;

 protected:
  virtual ~ImeKeyEventDispatcher() = default;
};

}  // namespace ui

#endif  // UI_BASE_IME_IME_KEY_EVENT_DISPATCHER_H_

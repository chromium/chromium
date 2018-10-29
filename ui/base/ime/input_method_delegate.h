// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_DELEGATE_H_
#define UI_BASE_IME_INPUT_METHOD_DELEGATE_H_

#include "base/callback_forward.h"
#include "ui/base/ime/ui_base_ime_export.h"

namespace ui {

class KeyEvent;

struct EventDispatchDetails;

namespace internal {

// An interface implemented by the object that handles events sent back from an
// ui::InputMethod implementation.
class UI_BASE_IME_EXPORT InputMethodDelegate {
 public:
  virtual ~InputMethodDelegate() {}

  // Dispatch a key event already processed by the input method. Returns the
  // status of processing, as well as running the callback |ack_callback| with
  // the result of processing. |ack_callback| may be run asynchronously (if the
  // delegate does processing async). |ack_callback| may not be null.
  // Subclasses can use CallDispatchKeyEventPostIMEAck() to run the callback.
  virtual EventDispatchDetails DispatchKeyEventPostIME(
      KeyEvent* key_event,
      base::OnceCallback<void(bool)> ack_callback) = 0;

 protected:
  static void CallDispatchKeyEventPostIMEAck(
      KeyEvent* key_event,
      base::OnceCallback<void(bool)> ack_callback);
};

}  // namespace internal
}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_DELEGATE_H_

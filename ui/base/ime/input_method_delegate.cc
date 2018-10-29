// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_delegate.h"

#include "base/callback.h"
#include "ui/events/event.h"

namespace ui {
namespace internal {

// static
void InputMethodDelegate::CallDispatchKeyEventPostIMEAck(
    KeyEvent* key_event,
    base::OnceCallback<void(bool)> ack_callback) {
  if (ack_callback)
    std::move(ack_callback).Run(key_event->stopped_propagation());
}

}  // namespace internal
}  // namespace ui

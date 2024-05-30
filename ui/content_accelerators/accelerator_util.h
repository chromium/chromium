// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CONTENT_ACCELERATORS_ACCELERATOR_UTIL_H_
#define UI_CONTENT_ACCELERATORS_ACCELERATOR_UTIL_H_

#include "components/input/native_web_keyboard_event.h"
#include "ui/base/accelerators/accelerator.h"

namespace ui {

// Returns |event| as a ui::Accelerator.
ui::Accelerator GetAcceleratorFromNativeWebKeyboardEvent(
    const input::NativeWebKeyboardEvent& event);

}  // namespace ui

#endif  // UI_CONTENT_ACCELERATORS_ACCELERATOR_UTIL_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CONSTANTS_H_
#define UI_BASE_IME_CONSTANTS_H_

#include "ui/base/ime/ui_base_ime_export.h"

namespace ui {

// The name of the property that is attach to the key event and indicates
// whether it was from the virtual keyboard.
// This is used where the key event is simulated by the virtual keyboard
// (e.g. IME extension API) as well as the input field implementation (e.g.
// Textfield).
UI_BASE_IME_EXPORT extern const char kPropertyFromVK[];

}  // namespace ui

#endif  // UI_BASE_IME_CONSTANTS_H_

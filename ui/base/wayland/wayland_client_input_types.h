// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WAYLAND_WAYLAND_CLIENT_INPUT_TYPES_H_
#define UI_BASE_WAYLAND_WAYLAND_CLIENT_INPUT_TYPES_H_

#include <text-input-extension-unstable-v1-client-protocol.h>

#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"

namespace ui::wayland {

zcr_extended_text_input_v1_input_type ConvertFromTextInputType(
    TextInputType text_input_type);

zcr_extended_text_input_v1_input_mode ConvertFromTextInputMode(
    TextInputMode text_input_mode);

uint32_t ConvertFromTextInputFlags(uint32_t text_input_flags);

}  // namespace ui::wayland

#endif  // UI_BASE_WAYLAND_WAYLAND_CLIENT_INPUT_TYPES_H_

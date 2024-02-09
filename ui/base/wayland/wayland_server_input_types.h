// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WAYLAND_WAYLAND_SERVER_INPUT_TYPES_H_
#define UI_BASE_WAYLAND_WAYLAND_SERVER_INPUT_TYPES_H_

#include <text-input-extension-unstable-v1-server-protocol.h>

#include <optional>

#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"

namespace ui::wayland {

// Coverts zcr_extended_text_input::input_type into ui::TextInputType.
// Returns nullopt if unknown type is given.
// This can happen if wayland client (e.g. Lacros) and wayland compositor
// (e.g. exo) have version skew, so that the wayland client sends a new
// type that the wayland compositor cannot understand.
std::optional<TextInputType> ConvertToTextInputType(
    zcr_extended_text_input_v1_input_type wayland_input_type);

// Converts zcr_extended_text_input::input_mode into ui::TextInputMode.
std::optional<TextInputMode> ConvertToTextInputMode(
    zcr_extended_text_input_v1_input_mode wayland_input_mode);

// Converts a bit set of ui::TextInputFlags into a bit set of
// zcr_extended_text_input::input_flags.
// Returns a pair of (converted result, unrecognized flags).
std::pair<uint32_t, uint32_t> ConvertToTextInputFlags(
    uint32_t wayland_input_flags);

}  // namespace ui::wayland

#endif  // UI_BASE_WAYLAND_WAYLAND_SERVER_INPUT_TYPES_H_

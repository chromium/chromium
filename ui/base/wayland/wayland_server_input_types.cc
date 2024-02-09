// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/wayland/wayland_server_input_types.h"

#include "ui/base/wayland/wayland_input_types_impl.h"

namespace ui::wayland {

std::optional<TextInputType> ConvertToTextInputType(
    zcr_extended_text_input_v1_input_type wayland_input_type) {
  switch (wayland_input_type) {
#define MAP_ENTRY(name)                              \
  case ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_TYPE_##name: \
    return ui::TEXT_INPUT_TYPE_##name;

    MAP_TYPES(MAP_ENTRY)
#undef MAP_ENTRY
  }

  return std::nullopt;
}

std::optional<TextInputMode> ConvertToTextInputMode(
    zcr_extended_text_input_v1_input_mode wayland_input_mode) {
  switch (wayland_input_mode) {
#define MAP_ENTRY(name)                              \
  case ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_MODE_##name: \
    return ui::TEXT_INPUT_MODE_##name;

    MAP_MODES(MAP_ENTRY)
#undef MAP_ENTRY
  }

  return std::nullopt;
}

std::pair<uint32_t, uint32_t> ConvertToTextInputFlags(
    uint32_t wayland_input_flags) {
  uint32_t result = 0;
  for (const auto ui_flag : kAllTextInputFlags) {
    const uint32_t wayland_flag = ConvertFromTextInputFlag(ui_flag);
    if (wayland_input_flags & wayland_flag) {
      result |= ui_flag;
      wayland_input_flags &= ~wayland_flag;
    }
  }
  return {result, wayland_input_flags};
}

}  // namespace ui::wayland

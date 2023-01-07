// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/wayland/wayland_client_input_types.h"

#include "base/check_op.h"
#include "ui/base/wayland/wayland_input_types_impl.h"

namespace ui::wayland {

zcr_extended_text_input_v1_input_type ConvertFromTextInputType(
    TextInputType text_input_type) {
  switch (text_input_type) {
#define MAP_ENTRY(name)            \
  case ui::TEXT_INPUT_TYPE_##name: \
    return ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_TYPE_##name;

    MAP_TYPES(MAP_ENTRY)
#undef MAP_ENTRY
  }
}

zcr_extended_text_input_v1_input_mode ConvertFromTextInputMode(
    TextInputMode text_input_mode) {
  switch (text_input_mode) {
#define MAP_ENTRY(name)            \
  case ui::TEXT_INPUT_MODE_##name: \
    return ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_MODE_##name;

    MAP_MODES(MAP_ENTRY)
#undef MAP_ENTRY
  }
}

uint32_t ConvertFromTextInputFlags(uint32_t text_input_flags) {
  uint32_t result = 0;
  for (auto ui_flag : kAllTextInputFlags) {
    if (text_input_flags & ui_flag) {
      result |= ConvertFromTextInputFlag(ui_flag);
      text_input_flags &= ~ui_flag;
    }
  }

  // Making sure all the bits are converted.
  DCHECK_EQ(text_input_flags, 0u);
  return result;
}

}  // namespace ui::wayland

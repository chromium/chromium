// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_X11_XINPUT_UTIL_H_
#define UI_EVENTS_DEVICES_X11_XINPUT_UTIL_H_

#include <cstdint>

#include "base/containers/span.h"
#include "ui/events/devices/x11/events_devices_x11_export.h"

namespace ui {

EVENTS_DEVICES_X11_EXPORT
inline void SetXinputMask(base::span<uint8_t> mask, unsigned int opcode) {
  size_t byte_index = opcode / 8;
  const auto bit = 1 << (opcode & 7);
  mask[byte_index] |= static_cast<uint8_t>(bit);
}

EVENTS_DEVICES_X11_EXPORT
inline bool IsXinputMaskSet(base::span<const uint8_t> mask,
                            unsigned int opcode) {
  size_t byte_index = opcode / 8;
  if (byte_index >= mask.size()) {
    return false;
  }
  const auto bit = 1 << (opcode & 7);
  return mask[byte_index] & static_cast<uint8_t>(bit);
}

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_X11_XINPUT_UTIL_H_

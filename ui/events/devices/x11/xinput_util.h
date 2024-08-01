// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef UI_EVENTS_DEVICES_X11_XINPUT_UTIL_H_
#define UI_EVENTS_DEVICES_X11_XINPUT_UTIL_H_

#include <cstdint>

#include "ui/events/devices/x11/events_devices_x11_export.h"

namespace ui {

EVENTS_DEVICES_X11_EXPORT
inline void SetXinputMask(void* mask, unsigned int opcode) {
  const auto bit = 1 << (opcode & 7);
  static_cast<uint8_t*>(mask)[opcode / 8] |= bit;
}

EVENTS_DEVICES_X11_EXPORT
inline bool IsXinputMaskSet(const void* mask, unsigned int opcode) {
  const auto bit = 1 << (opcode & 7);
  return static_cast<const uint8_t*>(mask)[opcode / 8] & bit;
}

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_X11_XINPUT_UTIL_H_

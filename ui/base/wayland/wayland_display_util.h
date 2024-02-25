// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WAYLAND_WAYLAND_DISPLAY_UTIL_H_
#define UI_BASE_WAYLAND_WAYLAND_DISPLAY_UTIL_H_

#include <cstdint>

namespace ui::wayland {

struct WaylandDisplayIdPair {
  uint32_t high;
  uint32_t low;
};

// Convert int64_t display id into pair of uint32_t.
WaylandDisplayIdPair ToWaylandDisplayIdPair(int64_t display_id);

// Convert pair of uint32_t into display id int64_t.
int64_t FromWaylandDisplayIdPair(WaylandDisplayIdPair&& display_id_pair);

}  // namespace ui::wayland

#endif  // UI_BASE_WAYLAND_WAYLAND_DISPLAY_UTIL_H_

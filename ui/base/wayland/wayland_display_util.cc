// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/wayland/wayland_display_util.h"

namespace ui::wayland {

WaylandDisplayIdPair ToWaylandDisplayIdPair(int64_t display_id) {
  return {static_cast<uint32_t>(display_id >> 32),
          static_cast<uint32_t>(display_id)};
}

int64_t FromWaylandDisplayIdPair(WaylandDisplayIdPair&& display_id_pair) {
  return static_cast<int64_t>(display_id_pair.high) << 32 |
         static_cast<int64_t>(display_id_pair.low);
}

}  // namespace ui::wayland

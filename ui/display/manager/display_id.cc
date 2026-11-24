// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_id.h"

#include <ostream>

namespace display::internal {

std::ostream& operator<<(std::ostream& os, const DisplayId& display_id) {
  return os << "{port_id=" << display_id.port_id
            << ", edid_display_id=" << display_id.edid_display_id
            << ", uses_edid_display_id=" << display_id.uses_edid_display_id
            << "}";
}

}  // namespace display::internal

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DISPLAY_ID_H_
#define UI_DISPLAY_MANAGER_DISPLAY_ID_H_

#include <stdint.h>

#include <iosfwd>

#include "ui/display/manager/display_manager_export.h"
#include "ui/display/types/display_constants.h"

namespace display::internal {

// Identifies a display by both its port-based display ID and its EDID-based
// display ID. This is internal to //ui/display/manager and //ash/display.
// Everything else uses the int64_t display ID returned by `runtime_id()`, and
// the display manager takes care of the mapping.
struct DisplayId {
  // Port-based display ID, which depends on the connector the display is
  // attached to.
  int64_t port_id = kInvalidDisplayId;
  // EDID-based display ID (see EdidParser::GetEdidBasedDisplayId()), or
  // kInvalidDisplayId if not available.
  int64_t edid_display_id = kInvalidDisplayId;
  // Whether `edid_display_id` is used as the runtime display ID. It's false
  // when EDID-based display IDs are disabled. `edid_display_id` is still kept
  // so that data stored under it (e.g. while the feature was enabled) can be
  // found.
  bool uses_edid_display_id = true;

  bool has_edid_display_id() const {
    return edid_display_id != kInvalidDisplayId;
  }

  // The display ID used at runtime, e.g. display::Display::id().
  int64_t runtime_id() const {
    return has_edid_display_id() && uses_edid_display_id ? edid_display_id
                                                         : port_id;
  }

  // The display ID of this display that isn't the runtime ID, or
  // kInvalidDisplayId if the display has no EDID-based display ID. Data stored
  // under this ID can be carried forward to the runtime ID.
  int64_t other_id() const {
    if (!has_edid_display_id()) {
      return kInvalidDisplayId;
    }
    return uses_edid_display_id ? port_id : edid_display_id;
  }

  friend bool operator==(const DisplayId&, const DisplayId&) = default;
};

DISPLAY_MANAGER_EXPORT std::ostream& operator<<(std::ostream& os,
                                                const DisplayId& display_id);

}  // namespace display::internal

#endif  // UI_DISPLAY_MANAGER_DISPLAY_ID_H_

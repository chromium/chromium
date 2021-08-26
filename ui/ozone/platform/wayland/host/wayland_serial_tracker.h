// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SERIAL_TRACKER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SERIAL_TRACKER_H_

#include <cstdint>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ui {
class WaylandConnection;
}

namespace wl {

// Utility classes that help on tracking and retrieving "serial" values and
// meta-data, received through Wayland protocol events. Each serial value is
// associated with an event type, which is represented by a SerialType enum
// entry.

enum class SerialType {
  kMouseEnter,
  kMousePress,
  kTouchPress,
  kKeyPress,
  kMaxValue = kKeyPress
};

struct Serial {
  uint32_t value;
  SerialType type;
};

class SerialTracker final {
 public:
  explicit SerialTracker(ui::WaylandConnection* connection);
  SerialTracker(const SerialTracker&) = delete;
  SerialTracker& operator=(const SerialTracker&) = delete;
  ~SerialTracker();

  // Update/unset serial for a given |type|.
  void UpdateSerial(SerialType type, uint32_t serial);
  void ResetSerial(SerialType type);

  // Returns the current serial for a given |type|, if any.
  absl::optional<Serial> GetSerial(SerialType type) const;

  // Returns the serial value for any of the given |types|. The lookup
  // precedence is based on the types vector order, i.e: the first non-null
  // matching value is returned, if any.
  absl::optional<Serial> GetSerial(const std::vector<SerialType>& types) const;

 private:
  ui::WaylandConnection* const connection_;

  base::fixed_flat_map<SerialType,
                       absl::optional<Serial>,
                       static_cast<size_t>(SerialType::kMaxValue) + 1>
      serials_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SERIAL_TRACKER_H_

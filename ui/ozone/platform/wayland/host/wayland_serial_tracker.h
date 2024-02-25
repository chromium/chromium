// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SERIAL_TRACKER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SERIAL_TRACKER_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/time/time.h"

namespace wl {

// Utility classes that help on tracking and retrieving "serial" values and
// meta-data, received through Wayland protocol events. Each serial value is
// associated with an event type, which is represented by a SerialType enum
// entry.
//
// Note: SerialTracker must not be used to track related input object states,
// e.g: whether a pointer button is pressed, or whether there are active touch
// points. Instead, other specific APIs should be added/used for that.

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
  base::TimeDelta timestamp;
};

class SerialTracker final {
 public:
  SerialTracker();
  SerialTracker(const SerialTracker&) = delete;
  SerialTracker& operator=(const SerialTracker&) = delete;
  ~SerialTracker();

  // Update/unset serial for a given |type|.
  void UpdateSerial(SerialType type, uint32_t serial);
  void ResetSerial(SerialType type);

  // Returns the current serial for a given |type|, if any.
  std::optional<Serial> GetSerial(SerialType type) const;

  // Returns the most recent serial matching the given |types|, if any.
  std::optional<Serial> GetSerial(const std::vector<SerialType>& types) const;

  void ClearForTesting();

  std::string ToString() const;

 private:
  const base::TimeTicks base_time_;

  base::fixed_flat_map<SerialType,
                       std::optional<Serial>,
                       static_cast<size_t>(SerialType::kMaxValue) + 1>
      serials_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SERIAL_TRACKER_H_

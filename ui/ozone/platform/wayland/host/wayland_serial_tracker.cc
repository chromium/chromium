// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace wl {

namespace {

// TODO(crbug.com/1211874): Needed temporarily to keep components not yet
// migrated to SerialTracker working. Clean up when the migration is complete.
void UpdateConnectionSerial(ui::WaylandConnection* connection,
                            SerialType type,
                            uint32_t serial) {
  switch (type) {
    case SerialType::kMouseEnter:
      connection->set_serial(serial, ui::ET_MOUSE_ENTERED);
      return;
    case SerialType::kMousePress:
      connection->set_serial(serial, ui::ET_MOUSE_PRESSED);
      return;
    case SerialType::kTouchPress:
      connection->set_serial(serial, ui::ET_TOUCH_PRESSED);
      return;
    case SerialType::kKeyPress:
      connection->set_serial(serial, ui::ET_KEY_PRESSED);
      return;
  }
}

}  // namespace

SerialTracker::SerialTracker(ui::WaylandConnection* connection)
    : connection_(connection),
      serials_(base::MakeFixedFlatMap<SerialType, absl::optional<Serial>>({
          {SerialType::kMouseEnter, absl::nullopt},
          {SerialType::kMousePress, absl::nullopt},
          {SerialType::kTouchPress, absl::nullopt},
          {SerialType::kKeyPress, absl::nullopt},
      })) {}

SerialTracker::~SerialTracker() = default;

void SerialTracker::UpdateSerial(SerialType type, uint32_t serial) {
  DCHECK(base::Contains(serials_, type));
  serials_.at(type) = {serial, type};
  UpdateConnectionSerial(connection_, type, serial);
}

void SerialTracker::ResetSerial(SerialType type) {
  DCHECK(base::Contains(serials_, type));
  serials_.at(type) = absl::nullopt;
  UpdateConnectionSerial(connection_, type, 0u);
}

absl::optional<Serial> SerialTracker::GetSerial(SerialType type) const {
  DCHECK(base::Contains(serials_, type));
  return serials_.at(type);
}

absl::optional<Serial> SerialTracker::GetSerial(
    const std::vector<SerialType>& types) const {
  DCHECK(!types.empty());
  for (const auto& type : types) {
    if (auto serial = GetSerial(type))
      return serial;
  }
  return absl::nullopt;
}

}  // namespace wl

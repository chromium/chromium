// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"

#include <optional>
#include <sstream>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/time/time.h"

namespace wl {

SerialTracker::SerialTracker()
    : base_time_(base::TimeTicks::Now()),
      serials_(base::MakeFixedFlatMap<SerialType, std::optional<Serial>>({
          {SerialType::kMouseEnter, std::nullopt},
          {SerialType::kMousePress, std::nullopt},
          {SerialType::kTouchPress, std::nullopt},
          {SerialType::kKeyPress, std::nullopt},
      })) {}

SerialTracker::~SerialTracker() = default;

void SerialTracker::UpdateSerial(SerialType type, uint32_t serial) {
  DCHECK(base::Contains(serials_, type));
  serials_.at(type) = {.value = serial,
                       .type = type,
                       .timestamp = base::TimeTicks::Now() - base_time_};
}

void SerialTracker::ResetSerial(SerialType type) {
  DCHECK(base::Contains(serials_, type));
  serials_.at(type) = std::nullopt;
}

std::optional<Serial> SerialTracker::GetSerial(SerialType type) const {
  DCHECK(base::Contains(serials_, type));
  return serials_.at(type);
}

std::optional<Serial> SerialTracker::GetSerial(
    const std::vector<SerialType>& types) const {
  DCHECK(!types.empty());
  std::optional<Serial> most_recent;
  for (const auto& type : types) {
    if (auto serial = GetSerial(type)) {
      if (!most_recent || serial->timestamp > most_recent->timestamp)
        most_recent = serial;
    }
  }
  return most_recent;
}

void SerialTracker::ClearForTesting() {
  ResetSerial(SerialType::kMouseEnter);
  ResetSerial(SerialType::kMousePress);
  ResetSerial(SerialType::kTouchPress);
  ResetSerial(SerialType::kKeyPress);
}

std::string SerialTracker::ToString() const {
  auto tostring = [this](const SerialType serial_type, const std::string& label,
                         std::ostringstream& out) {
    auto serial = GetSerial(serial_type);
    out << label;
    if (!serial) {
      out << "<none>";
    } else {
      out << "tracker_id=" << serial->value << ", time=" << serial->timestamp;
    };
  };
  std::ostringstream out;
  tostring(SerialType::kMouseEnter, "mouse_enter: ", out);
  tostring(SerialType::kMousePress, ", mouse_press: ", out);
  tostring(SerialType::kTouchPress, ", touch_press: ", out);
  tostring(SerialType::kKeyPress, ", key_press: ", out);
  return out.str();
}

}  // namespace wl

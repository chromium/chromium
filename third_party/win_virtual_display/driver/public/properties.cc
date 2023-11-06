// Copyright (c) Microsoft Corporation

#include "third_party/win_virtual_display/driver/public/properties.h"

#include <algorithm>
#include <iterator>

namespace display::test {
const MonitorMode MonitorMode::k1024x768 = MonitorMode(1024, 768);
const MonitorMode MonitorMode::k1920x1080 = MonitorMode(1920, 1080);

DriverProperties::DriverProperties(const std::vector<MonitorMode>& modes) {
  requested_modes_size_ = std::min(modes.size(), kMaxMonitors);
  std::copy_n(modes.begin(), requested_modes_size_, requested_modes_.begin());
}

// Return a vector of the requested monitor configurations.
std::vector<MonitorMode> DriverProperties::requested_modes() const {
  std::vector<MonitorMode> vector;
  std::copy_n(requested_modes_.begin(), requested_modes_size_,
              std::back_inserter(vector));
  return vector;
}
}  // namespace display::test

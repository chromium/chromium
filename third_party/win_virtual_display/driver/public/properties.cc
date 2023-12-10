// Copyright (c) Microsoft Corporation

#include "third_party/win_virtual_display/driver/public/properties.h"

#include <algorithm>
#include <iterator>

namespace display::test {
const MonitorConfig MonitorConfig::k1024x768 = MonitorConfig(1024, 768);
const MonitorConfig MonitorConfig::k1920x1080 = MonitorConfig(1920, 1080);

DriverProperties::DriverProperties(const std::vector<MonitorConfig>& modes) {
  requested_configs_size_ = std::min(modes.size(), kMaxMonitors);
  std::copy_n(modes.begin(), requested_configs_size_,
              requested_configs_.begin());
}

// Return a vector of the requested monitor configurations.
std::vector<MonitorConfig> DriverProperties::requested_configs() const {
  std::vector<MonitorConfig> vector;
  std::copy_n(requested_configs_.begin(), requested_configs_size_,
              std::back_inserter(vector));
  return vector;
}
}  // namespace display::test

// Copyright (c) Microsoft Corporation

#ifndef THIRD_PARTY_WIN_VIRTUAL_DISPLAY_DRIVER_PUBLIC_PROPERTIES_H_
#define THIRD_PARTY_WIN_VIRTUAL_DISPLAY_DRIVER_PUBLIC_PROPERTIES_H_

#if defined(_MSC_VER)
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#endif

#include <windows.h>

// Must come before Devpropdef.h
#include <initguid.h>

#include <Devpropdef.h>

#include <array>
#include <vector>

namespace display::test {
// Defines a GUID to uniquely identify the unified property for display
// configuration. See:
// https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/accessing-the-unified-device-property-model
DEFINE_DEVPROPKEY(DisplayConfigurationProperty,
                  0x1d229ffd,
                  0x6765,
                  0x497a,
                  0x83,
                  0xad,
                  0x4a,
                  0xa3,
                  0x85,
                  0x5d,
                  0x2f,
                  0xf7,
                  2);

// Holds configuration for a virtual display.
class MonitorMode {
 public:
  // List of supported monitor modes.
  static const MonitorMode k1024x768;
  static const MonitorMode k1920x1080;

  MonitorMode() = default;
  // Get width (horizontal resolution).
  unsigned short width() const { return width_; }
  // Get height (vertical resolution).
  unsigned short height() const { return height_; }
  // Get vertical sync (refresh rate).
  unsigned short v_sync() const { return v_sync_; }

 private:
  // Private constructor. Use the defined constants for supported modes.
  constexpr MonitorMode(unsigned short width,
                        unsigned short height,
                        unsigned short v_sync = 60)
      : width_(width), height_(height), v_sync_(v_sync) {}
  unsigned short width_;   // Horizontal resolution.
  unsigned short height_;  // Vertical resolution.
  unsigned short v_sync_;  // Vertical sync (refresh rate).
};

// Define the corresponding structure associated with the
// `DisplayConfigurationProperty` property key.
class DriverProperties {
 public:
  // Maximum number of virtual displays that may be created.
  static constexpr size_t kMaxMonitors = 10;
  DriverProperties() = default;
  // Create monitors with the specified modes. If the length of `modes` is
  // greater than `kMaxMonitors`, the modes are truncated to `kMaxMonitors`.
  explicit DriverProperties(const std::vector<MonitorMode>& modes);
  // Return a vector of the requested monitor configurations.
  std::vector<MonitorMode> requested_modes() const;

 private:
  // std::vector doesn't work here due to serialization & Windows API
  // limitations so a fixed array is used instead.
  std::array<MonitorMode, kMaxMonitors> requested_modes_ = {};
  size_t requested_modes_size_ = 0;
};

}  // namespace display::test

#endif  // THIRD_PARTY_WIN_VIRTUAL_DISPLAY_DRIVER_PUBLIC_PROPERTIES_H_

// Copyright (c) Microsoft Corporation

#ifndef THIRD_PARTY_WIN_VIRTUAL_DISPLAY_DRIVER_PUBLIC_PROPERTIES_H_
#define THIRD_PARTY_WIN_VIRTUAL_DISPLAY_DRIVER_PUBLIC_PROPERTIES_H_

// Must come before Devpropdef.h
#include <initguid.h>

#include <Devpropdef.h>

#include <array>

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

// Define the corresponding structure associated with the
// `DisplayConfigurationProperty` property key.
struct DriverProperties {
  // Maximum number of virtual displays that may be created.
  static constexpr size_t kMaxMonitors = 10;
  struct MonitorMode {
    unsigned short width;
    unsigned short height;
    unsigned char vSync;
  };
  // TODO: Add more supported modes
  static constexpr size_t kSupportedModesCount = 2;
  static constexpr MonitorMode kSupportedModes[kSupportedModesCount] = {
      {1024, 768, 120},
      {1920, 1080, 60}};
  std::array<MonitorMode, kMaxMonitors> requested_modes = {};
  int monitor_count = 0;
};

#endif  // THIRD_PARTY_WIN_VIRTUAL_DISPLAY_DRIVER_PUBLIC_PROPERTIES_H_

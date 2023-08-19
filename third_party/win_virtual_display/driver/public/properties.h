// Copyright (c) Microsoft Corporation

#ifndef THIRD_PARTY_WIN_VIRTUAL_DISPLAY_DRIVER_PUBLIC_PROPERTIES_H_
#define THIRD_PARTY_WIN_VIRTUAL_DISPLAY_DRIVER_PUBLIC_PROPERTIES_H_

// Must come before Devpropdef.h
#include <initguid.h>

#include <Devpropdef.h>

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
  explicit DriverProperties(int n) { num_displays = n; }
  int num_displays;
};

#endif  // THIRD_PARTY_WIN_VIRTUAL_DISPLAY_DRIVER_PUBLIC_PROPERTIES_H_

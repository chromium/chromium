// Copyright (c) Microsoft Corporation

#include "base/win/windows_full.h"
#include "base/win/windows_types.h"
#include "third_party/win_virtual_display/driver/public/properties.h"

#include <conio.h>
#include <swdevice.h>
#include <wrl.h>
#include <cstdio>
#include <limits>
#include <vector>

VOID WINAPI CreationCallback(_In_ HSWDEVICE hSwDevice,
                             _In_ HRESULT hrCreateResult,
                             _In_opt_ PVOID pContext,
                             _In_opt_ PCWSTR pszDeviceInstanceId) {
  HANDLE hEvent = *(HANDLE*)pContext;

  SetEvent(hEvent);
  UNREFERENCED_PARAMETER(hSwDevice);
  UNREFERENCED_PARAMETER(hrCreateResult);
  UNREFERENCED_PARAMETER(pszDeviceInstanceId);
}

int __cdecl main(int argc, char** argv) {
  UNREFERENCED_PARAMETER(argc);
  UNREFERENCED_PARAMETER(argv);

  HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  HSWDEVICE hSwDevice;
  SW_DEVICE_CREATE_INFO createInfo = {0};
  PCWSTR description = L"Chromium Virtual Display Driver";

  // These match the Pnp id's in the inf file so OS will load the driver when
  // the device is created
  PCWSTR instanceId = L"ChromiumVirtualDisplayDriver";
  PCWSTR hardwareIds = L"ChromiumVirtualDisplayDriver\0\0";
  PCWSTR compatibleIds = L"ChromiumVirtualDisplayDriver\0\0";

  createInfo.cbSize = sizeof(createInfo);
  createInfo.pszzCompatibleIds = compatibleIds;
  createInfo.pszInstanceId = instanceId;
  createInfo.pszzHardwareIds = hardwareIds;
  createInfo.pszDeviceDescription = description;

  createInfo.CapabilityFlags = SWDeviceCapabilitiesRemovable |
                               SWDeviceCapabilitiesSilentInstall |
                               SWDeviceCapabilitiesDriverRequired;

  // Set configuration properties to send to the driver.
  display::test::DriverProperties p;

  DEVPROPERTY properties[1];
  DEVPROPERTY& property = properties[0];
  property.Type = DEVPROP_TYPE_BINARY;
  property.CompKey.Store = DEVPROP_STORE_SYSTEM;
  property.CompKey.Key = display::test::DisplayConfigurationProperty;
  property.CompKey.LocaleName = NULL;
  property.BufferSize = sizeof(display::test::DriverProperties);
  property.Buffer = &p;

  // Create the device
  HRESULT hr = SwDeviceCreate(L"ChromiumVirtualDisplayDriver",
                              L"HTREE\\ROOT\\0", &createInfo, 1, properties,
                              CreationCallback, &hEvent, &hSwDevice);
  if (FAILED(hr)) {
    printf("SwDeviceCreate failed with 0x%lx\n", hr);
    return 1;
  }

  // Wait for callback to signal that the device has been created
  printf("Waiting for device to be created....\n");
  DWORD waitResult = WaitForSingleObject(hEvent, 10 * 1000);
  if (waitResult != WAIT_OBJECT_0) {
    printf("Wait for device creation failed\n");
    return 1;
  }
  printf("Device created\n\n");

  bool bExit = false;
  int monitor_id = 0;
  do {
    printf(
        "Press 'x' to exit and destroy the software device, or 'a' to add a "
        "1024x768 monitor, 'A' to add a 1920x1080 monitor, or enter a monitor "
        "ID from the following list to destroy it:\n");
    std::vector<display::test::MonitorConfig> current_configs =
        p.requested_configs();
    for (const auto& requested : current_configs) {
      printf("%i\n", requested.product_code());
    }
    // Wait for key press
    int key = _getch();

    if (key == 'x' || key == 'X') {
      bExit = true;
    }
    if (key == 'a' || key == 'A') {
      if (current_configs.size() >=
          display::test::DriverProperties::kMaxMonitors) {
        printf("Error: Max number of monitors reached.");
        break;
      }
      auto mode = display::test::MonitorConfig::k1024x768;
      if (key == 'A') {
        mode = display::test::MonitorConfig::k1920x1080;
      }
      mode.set_product_code((monitor_id++) %
                            std::numeric_limits<unsigned short>::max());
      current_configs.push_back(mode);
      p = display::test::DriverProperties(current_configs);
      property.Buffer = &p;
      SwDevicePropertySet(hSwDevice, 1, properties);
      printf("Properties updated.\n");
    }
    if (key >= '0' && key <= '9') {
      std::erase_if(current_configs,
                    [&](auto& c) { return c.product_code() == key - '0'; });
      p = display::test::DriverProperties(current_configs);
      property.Buffer = &p;
      SwDevicePropertySet(hSwDevice, 1, properties);
      printf("Properties updated.\n");
    }
  } while (!bExit);

  // Stop the device, this will cause the sample to be unloaded
  SwDeviceClose(hSwDevice);

  return 0;
}

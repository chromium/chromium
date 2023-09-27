// Copyright (c) Microsoft Corporation

#include "base/win/windows_full.h"
#include "base/win/windows_types.h"
#include "third_party/win_virtual_display/driver/public/properties.h"

#include <conio.h>
#include <swdevice.h>
#include <wrl.h>
#include <cstdio>
#include <iostream>
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

  int input = -1;
  printf("Enter the number of virtual displays to create (0 - %zu): ",
         DriverProperties::kMaxMonitors);
  int success = scanf("%d", &input);
  if (success != 1 || input <= 0 ||
      input > static_cast<int>(DriverProperties::kMaxMonitors)) {
    printf("\n An error occurred while taking input for # of displays");
    return 0;
  }
  size_t display_count = static_cast<size_t>(input);

  // Set configuration properties to send to the driver.
  DriverProperties p;

  printf("The supported monitor modes (Width x Height, VSync) are: \n");
  for (size_t i = 0; i < DriverProperties::kSupportedModesCount; i++) {
    printf("%zu (%hu, %hu, %hu)\n", i, p.kSupportedModes[i].width,
           p.kSupportedModes[i].height, p.kSupportedModes[i].vSync);
  }

  for (size_t i = 0; i < display_count; i++) {
    printf("Choose the mode for display #%zu: ", i);
    int index_mode;
    success = scanf("%d", &index_mode);
    if (success != 1 || index_mode < 0 ||
        index_mode >=
            static_cast<int>(DriverProperties::kSupportedModesCount)) {
      printf("\n An error occurred while taking input for display mode");
      return 0;
    }
    p.requested_modes[p.monitor_count++] =
        DriverProperties::kSupportedModes[index_mode];
  }

  DEVPROPERTY properties[1];
  DEVPROPERTY& property = properties[0];
  property.Type = DEVPROP_TYPE_BINARY;
  property.CompKey.Store = DEVPROP_STORE_SYSTEM;
  property.CompKey.Key = DisplayConfigurationProperty;
  property.CompKey.LocaleName = NULL;
  property.BufferSize = sizeof(DriverProperties);
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

  // Now wait for user to indicate the device should be stopped
  printf("Press 'x' to exit and destory the software device\n");
  bool bExit = false;
  do {
    // Wait for key press
    int key = _getch();

    if (key == 'x' || key == 'X') {
      bExit = true;
    }
  } while (!bExit);

  // Stop the device, this will cause the sample to be unloaded
  SwDeviceClose(hSwDevice);

  return 0;
}

// Copyright (c) Microsoft Corporation

#include "base/win/windows_full.h"
#include "base/win/windows_types.h"
#include "third_party/win_virtual_display/driver/public/properties.h"

#include <conio.h>
#include <swdevice.h>
#include <wrl.h>
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

  // Set configuration properties to send to the driver.
  DriverProperties p(/*num_displays=*/2);
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

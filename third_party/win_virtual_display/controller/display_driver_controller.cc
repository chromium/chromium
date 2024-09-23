// Copyright (c) Microsoft Corporation

#include "third_party/win_virtual_display/controller/display_driver_controller.h"

#include <windows.h>

#include <devguid.h>
#include <setupapi.h>
#include <swdevice.h>
#include <wrl.h>
#include <cstdio>
#include "base/logging.h"

namespace display::test {
namespace {

// These values should match the corresponding values in the driver .inf file.
constexpr wchar_t kDriverName[] = L"ChromiumVirtualDisplayDriver";
// Dual null terminated for win32 API list.
constexpr wchar_t kDriverNameList[] = L"ChromiumVirtualDisplayDriver\0\0";
constexpr wchar_t kDriverDeviceName[] = L"ChromiumVirtualDisplayDriver Device";
constexpr wchar_t kDriverManufacturer[] = L"Chromium";
constexpr wchar_t kDriverDescription[] = L"Chromium Virtual Display Driver";

VOID WINAPI CreationCallback(_In_ HSWDEVICE hSwDevice,
                             _In_ HRESULT hrCreateResult,
                             _In_opt_ PVOID pContext,
                             _In_opt_ PCWSTR pszDeviceInstanceId) {
  HANDLE hEvent = *(HANDLE*)pContext;
  if (!SetEvent(hEvent)) {
    LOG(ERROR) << "SetEvent failed: " << GetLastError();
  }
}

// Builds an array of DEVPROPERTY based off the specified config.
std::array<DEVPROPERTY, 1> BuildDevProperties(DriverProperties& config) {
  std::array<DEVPROPERTY, 1> properties;
  DEVPROPERTY& property = properties[0];
  property.Type = DEVPROP_TYPE_BINARY;
  property.CompKey.Store = DEVPROP_STORE_SYSTEM;
  property.CompKey.Key = DisplayConfigurationProperty;
  property.CompKey.LocaleName = NULL;
  property.BufferSize = sizeof(DriverProperties);
  property.Buffer = &config;
  return properties;
}

}  // namespace

DisplayDriverController::~DisplayDriverController() {
  Reset();
}

// static
bool DisplayDriverController::IsDriverInstalled() {
  HDEVINFO hdevinfo =
      SetupDiGetClassDevsW(&GUID_DEVCLASS_DISPLAY, NULL, NULL, 0);
  if (hdevinfo == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "SetupDiGetClassDevsW failed: " << hdevinfo;
    return false;
  }
  if (!SetupDiBuildDriverInfoList(hdevinfo, NULL, SPDIT_CLASSDRIVER)) {
    LOG(ERROR) << "SetupDiBuildDriverInfoList failed: " << GetLastError();
  }
  SP_DRVINFO_DATA_W drvdata;
  drvdata.cbSize = sizeof(SP_DRVINFO_DATA_W);
  for (DWORD index = 0; SetupDiEnumDriverInfoW(
           hdevinfo, NULL, SPDIT_CLASSDRIVER, index++, &drvdata);) {
    if (std::wstring(drvdata.Description) == kDriverDeviceName &&
        std::wstring(drvdata.MfgName) == kDriverManufacturer) {
      SetupDiDestroyDeviceInfoList(hdevinfo);
      return true;
    }
  }
  DWORD error = GetLastError();
  LOG_IF(ERROR, error != ERROR_NO_MORE_ITEMS && error != ERROR_SUCCESS)
      << "SetupDiEnumDriverInfoW failed: " << error;
  SetupDiDestroyDeviceInfoList(hdevinfo);
  return false;
}

bool DisplayDriverController::SetDisplayConfig(DriverProperties config) {
  if (device_handle_ == nullptr) {
    return Initialize(config);
  }
  std::array<DEVPROPERTY, 1> properties = BuildDevProperties(config);
  HRESULT hr =
      SwDevicePropertySet(device_handle_, properties.size(), properties.data());
  return !FAILED(hr);
}

void DisplayDriverController::Reset() {
  if (device_handle_ != nullptr) {
    SwDeviceClose(device_handle_);
    device_handle_ = nullptr;
  }
}

bool DisplayDriverController::Initialize(DriverProperties config) {
  HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  HSWDEVICE hSwDevice;
  SW_DEVICE_CREATE_INFO createInfo = {0};

  createInfo.cbSize = sizeof(createInfo);
  createInfo.pszzCompatibleIds = kDriverNameList;
  createInfo.pszInstanceId = kDriverName;
  createInfo.pszzHardwareIds = kDriverNameList;
  createInfo.pszDeviceDescription = kDriverDescription;

  createInfo.CapabilityFlags = SWDeviceCapabilitiesRemovable |
                               SWDeviceCapabilitiesSilentInstall |
                               SWDeviceCapabilitiesDriverRequired;

  std::array<DEVPROPERTY, 1> properties = BuildDevProperties(config);
  // Create the device
  HRESULT hr = SwDeviceCreate(kDriverName, L"HTREE\\ROOT\\0", &createInfo,
                              properties.size(), properties.data(),
                              CreationCallback, &hEvent, &hSwDevice);
  if (FAILED(hr)) {
    LOG(ERROR) << "SwDeviceCreate failed: " << std::hex << hr;
    return false;
  }
  // Wait for callback to signal that the device has been created
  DWORD waitResult = WaitForSingleObject(hEvent, 10 * 1000);
  if (waitResult != WAIT_OBJECT_0) {
    LOG(ERROR) << "WaitForSingleObject failed: " << waitResult;
    return false;
  }
  device_handle_ = hSwDevice;
  return true;
}

}  // namespace display::test

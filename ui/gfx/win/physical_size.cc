// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/physical_size.h"

#include <windows.h>
#include <setupapi.h>

#include <iostream>
#include <memory>

#include "base/check_op.h"
#include "base/memory/free_deleter.h"
#include "base/scoped_generic.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"

// This GUID {E6F07B5F-EE97-4A90-B076-33F57BF4EAA7} was taken from
// https://msdn.microsoft.com/en-us/library/windows/hardware/ff545901.aspx
const GUID GUID_DEVICEINTERFACE_MONITOR = {
    0xE6F07B5F,
    0xEE97,
    0x4A90,
    {0xB0, 0x76, 0x33, 0xF5, 0x7B, 0xF4, 0xEA, 0xA7}};

namespace {

struct DeviceInfoListScopedTraits {
  static HDEVINFO InvalidValue() { return INVALID_HANDLE_VALUE; }

  static void Free(HDEVINFO h) { SetupDiDestroyDeviceInfoList(h); }
};

bool GetSizeFromRegistry(HDEVINFO device_info_list,
                         SP_DEVINFO_DATA* device_info,
                         int* width_mm,
                         int* height_mm) {
  base::win::RegKey reg_key(SetupDiOpenDevRegKey(
      device_info_list, device_info, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ));
  if (!reg_key.Valid())
    return false;

  BYTE data[128];  // EDID block is exactly 128 bytes long.
  ZeroMemory(&data[0], sizeof(data));
  DWORD data_length = sizeof(data);
  LONG return_value =
      reg_key.ReadValue(L"EDID", &data[0], &data_length, nullptr);
  if (return_value != ERROR_SUCCESS)
    return false;

  // Byte 54 is the start of the first descriptor block, which contains the
  // required timing information with the highest preference, and 12 bytes
  // into that block is the size information.
  // 66: width least significant bits
  // 67: height least significant bits
  // 68: 4 bits for each of width and height most significant bits
  if (data[54] == 0)
    return false;
  const int w = ((data[68] & 0xF0) << 4) + data[66];
  const int h = ((data[68] & 0x0F) << 8) + data[67];

  if (w <= 0 || h <= 0)
    return false;

  *width_mm = w;
  *height_mm = h;

  return true;
}

bool GetInterfaceDetailAndDeviceInfo(
    HDEVINFO device_info_list,
    SP_DEVICE_INTERFACE_DATA* interface_data,
    std::unique_ptr<SP_DEVICE_INTERFACE_DETAIL_DATA, base::FreeDeleter>*
        interface_detail,
    SP_DEVINFO_DATA* device_info) {
  DCHECK_EQ(sizeof(*device_info), device_info->cbSize);
  DWORD buffer_size;
  // This call populates device_info. It will also fail, but if the error is
  // "insufficient buffer" then it will set buffer_size and we can call again
  // with an allocated buffer.
  SetupDiGetDeviceInterfaceDetail(device_info_list, interface_data, nullptr, 0,
                                  &buffer_size, device_info);
  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    return false;

  interface_detail->reset(
      reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(malloc(buffer_size)));
  (*interface_detail)->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
  return SetupDiGetDeviceInterfaceDetail(device_info_list, interface_data,
                                         interface_detail->get(), buffer_size,
                                         nullptr, nullptr) != 0;
}

}  // namespace

namespace gfx {

// The physical size information is only available by looking in the EDID block
// via setup. However setup has the device path and not the device name that we
// use to identify displays. Therefore after looking up a device via setup we
// need to find the display again via EnumDisplayDevices (matching device path
// to the device ID of the display's interface) so we can return the device name
// (available from the interface's attached monitor).
std::vector<PhysicalDisplaySize> GetPhysicalSizeForDisplays() {
  std::vector<PhysicalDisplaySize> out;

  base::ScopedGeneric<HDEVINFO, DeviceInfoListScopedTraits> device_info_list(
      SetupDiGetClassDevs(&GUID_DEVICEINTERFACE_MONITOR, nullptr, nullptr,
                          DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));

  if (!device_info_list.is_valid())
    return out;

  SP_DEVICE_INTERFACE_DATA interface_data = {};
  interface_data.cbSize = sizeof(interface_data);
  int interface_index = 0;
  while (SetupDiEnumDeviceInterfaces(device_info_list.get(), nullptr,
                                     &GUID_DEVICEINTERFACE_MONITOR,
                                     interface_index++, &interface_data)) {
    std::unique_ptr<SP_DEVICE_INTERFACE_DETAIL_DATA, base::FreeDeleter>
        interface_detail;
    SP_DEVINFO_DATA device_info = {};
    device_info.cbSize = sizeof(device_info);
    bool get_info_succeeded =
        GetInterfaceDetailAndDeviceInfo(device_info_list.get(), &interface_data,
                                        &interface_detail, &device_info);
    if (!get_info_succeeded)
      continue;

    DISPLAY_DEVICE display_device = {};
    display_device.cb = sizeof(display_device);
    int display_index = 0;
    while (EnumDisplayDevices(nullptr, display_index++, &display_device,
                              EDD_GET_DEVICE_INTERFACE_NAME)) {
      DISPLAY_DEVICE attached_device = {};
      attached_device.cb = sizeof(attached_device);
      int attached_index = 0;
      while (EnumDisplayDevices(display_device.DeviceName, attached_index++,
                                &attached_device,
                                EDD_GET_DEVICE_INTERFACE_NAME)) {
        wchar_t* attached_device_id = attached_device.DeviceID;
        wchar_t* setup_device_path = interface_detail->DevicePath;
        if (wcsicmp(attached_device_id, setup_device_path) == 0) {
          int width_mm;
          int height_mm;
          bool found = GetSizeFromRegistry(device_info_list.get(), &device_info,
                                           &width_mm, &height_mm);
          if (found) {
            out.push_back(
                PhysicalDisplaySize(base::WideToUTF8(display_device.DeviceName),
                                    width_mm, height_mm));
          }
          break;
        }
      }
    }
  }
  return out;
}

}  // namespace gfx

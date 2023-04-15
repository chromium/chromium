// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/test_support/fake_ipen_device_statics.h"

#include "base/no_destructor.h"

using ABI::Windows::Devices::Input::IPenDevice;

namespace views {

FakeIPenDeviceStatics::FakeIPenDeviceStatics() = default;
FakeIPenDeviceStatics::~FakeIPenDeviceStatics() = default;

// static
FakeIPenDeviceStatics* FakeIPenDeviceStatics::GetInstance() {
  static base::NoDestructor<FakeIPenDeviceStatics> instance;
  return instance.get();
}

Microsoft::WRL::ComPtr<ABI::Windows::Devices::Input::IPenDeviceStatics>
FakeIPenDeviceStatics::FakeIPenDeviceStaticsComPtr() {
  FakeIPenDeviceStatics* instance = GetInstance();
  return static_cast<
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Input::IPenDeviceStatics>>(
      instance);
}

HRESULT FakeIPenDeviceStatics::GetFromPointerId(UINT32 pointer_id,
                                                IPenDevice** result) {
  auto pen_device = pen_device_map_.find(pointer_id);
  if (pen_device == pen_device_map_.end()) {
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  }
  return pen_device->second.CopyTo(result);
}

void FakeIPenDeviceStatics::SimulatePenEventGenerated(
    UINT32 pointer_id,
    Microsoft::WRL::ComPtr<IPenDevice> pen_device) {
  pen_device_map_[pointer_id] = pen_device;
}

void FakeIPenDeviceStatics::SimulateAllPenDevicesRemoved() {
  pen_device_map_.clear();
}

}  // namespace views

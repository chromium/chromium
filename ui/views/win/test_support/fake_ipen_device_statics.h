// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_TEST_SUPPORT_FAKE_IPEN_DEVICE_STATICS_H_
#define UI_VIEWS_WIN_TEST_SUPPORT_FAKE_IPEN_DEVICE_STATICS_H_

#include <windows.devices.input.h>
#include <wrl.h>

#include <map>

namespace views {

class FakeIPenDeviceStatics final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Input::IPenDeviceStatics> {
 public:
  FakeIPenDeviceStatics();

  FakeIPenDeviceStatics(const FakeIPenDeviceStatics&) = delete;
  FakeIPenDeviceStatics& operator=(const FakeIPenDeviceStatics&) = delete;

  ~FakeIPenDeviceStatics() final;

  static FakeIPenDeviceStatics* GetInstance();
  static Microsoft::WRL::ComPtr<ABI::Windows::Devices::Input::IPenDeviceStatics>
  FakeIPenDeviceStaticsComPtr();

  // ABI::Windows::Devices::Input::IPenDeviceStatics:
  IFACEMETHODIMP GetFromPointerId(
      UINT32 pointer_id,
      ABI::Windows::Devices::Input::IPenDevice** pen_device) override;

  // Test methods
  void SimulatePenEventGenerated(
      UINT32 pointer_id,
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Input::IPenDevice>
          pen_device);
  void SimulateAllPenDevicesRemoved();

 private:
  // Map pointer_id to pen device.
  std::map<UINT32,
           Microsoft::WRL::ComPtr<ABI::Windows::Devices::Input::IPenDevice>>
      pen_device_map_;
};

}  // namespace views

#endif  //  UI_VIEWS_WIN_TEST_SUPPORT_FAKE_IPEN_DEVICE_STATICS_H_

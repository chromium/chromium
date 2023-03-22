// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_TEST_SUPPORT_FAKE_IPEN_DEVICE_H_
#define UI_VIEWS_WIN_TEST_SUPPORT_FAKE_IPEN_DEVICE_H_

#include <windows.devices.input.h>
#include <wrl.h>

#include <string>

namespace views {

class FakeIPenDevice final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Input::IPenDevice> {
 public:
  FakeIPenDevice();

  FakeIPenDevice(const FakeIPenDevice&) = delete;
  FakeIPenDevice& operator=(const FakeIPenDevice&) = delete;

  ~FakeIPenDevice() override;

  // ABI::Windows::Devices::Input::IPenDevice:
  IFACEMETHODIMP get_PenId(GUID* value) override;

  // Helper method for getting the pen device GUID as a string.
  std::string GetGuid() const;

 private:
  GUID guid_;
};

}  // namespace views

#endif  //  UI_VIEWS_WIN_TEST_SUPPORT_FAKE_IPEN_DEVICE_H_

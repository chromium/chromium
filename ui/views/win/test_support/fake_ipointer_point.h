// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_TEST_SUPPORT_FAKE_IPOINTER_POINT_H_
#define UI_VIEWS_WIN_TEST_SUPPORT_FAKE_IPOINTER_POINT_H_

#include <windows.devices.input.h>
#include <windows.ui.input.h>
#include <wrl.h>

#include "ui/views/win/test_support/fake_ipointer_point_properties.h"

namespace views {

// ABI::Windows::UI::Input::IPointerPoint fake implementation.
class FakeIPointerPoint final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::UI::Input::IPointerPoint> {
 public:
  FakeIPointerPoint();
  FakeIPointerPoint(bool throw_error_in_get_properties,
                    bool has_usage_throws_error = false,
                    bool get_usage_throws_error = false,
                    int tsn = 0,
                    int tvid = 0);
  explicit FakeIPointerPoint(FakeIPointerPointProperties* p);
  FakeIPointerPoint& operator=(const FakeIPointerPoint&) = delete;

  ~FakeIPointerPoint() override;

  HRESULT WINAPI get_Properties(
      ABI::Windows::UI::Input::IPointerPointProperties**
          pointer_point_properties) override;
  HRESULT WINAPI
  get_PointerDevice(ABI::Windows::Devices::Input::IPointerDevice**) override;
  HRESULT WINAPI get_Position(ABI::Windows::Foundation::Point*) override;
  HRESULT WINAPI get_RawPosition(ABI::Windows::Foundation::Point*) override;
  HRESULT WINAPI get_PointerId(UINT32*) override;
  HRESULT WINAPI get_FrameId(UINT32*) override;
  HRESULT WINAPI get_Timestamp(UINT64*) override;
  HRESULT WINAPI get_IsInContact(boolean*) override;

 private:
  bool throw_error_in_get_properties_ = false;
  Microsoft::WRL::ComPtr<FakeIPointerPointProperties> properties_;
};

}  // namespace views

#endif  //  UI_VIEWS_WIN_TEST_SUPPORT_FAKE_IPOINTER_POINT_H_

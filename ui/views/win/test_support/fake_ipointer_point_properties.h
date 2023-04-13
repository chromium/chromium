// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_TEST_SUPPORT_FAKE_IPOINTER_POINT_PROPERTIES_H_
#define UI_VIEWS_WIN_TEST_SUPPORT_FAKE_IPOINTER_POINT_PROPERTIES_H_

#include <windows.devices.input.h>
#include <windows.ui.input.h>
#include <wrl.h>

namespace views {

// ABI::Windows::UI::Input::IPointerPointProperties fake implementation.
class FakeIPointerPointProperties final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::UI::Input::IPointerPointProperties> {
 public:
  FakeIPointerPointProperties();
  FakeIPointerPointProperties(bool has_usage_throws_error,
                              bool get_usage_throws_error,
                              int tsn,
                              int tvid);
  FakeIPointerPointProperties& operator=(const FakeIPointerPointProperties&) =
      delete;

  ~FakeIPointerPointProperties() override;

  HRESULT STDMETHODCALLTYPE get_IsInverted(boolean*) override;
  HRESULT STDMETHODCALLTYPE get_Pressure(float*) override;
  HRESULT STDMETHODCALLTYPE get_IsEraser(boolean*) override;
  HRESULT STDMETHODCALLTYPE get_Orientation(float*) override;
  HRESULT STDMETHODCALLTYPE get_XTilt(float*) override;
  HRESULT STDMETHODCALLTYPE get_YTilt(float*) override;
  HRESULT STDMETHODCALLTYPE get_Twist(float*) override;
  HRESULT STDMETHODCALLTYPE
  get_ContactRect(ABI::Windows::Foundation::Rect*) override;
  HRESULT STDMETHODCALLTYPE
  get_ContactRectRaw(ABI::Windows::Foundation::Rect*) override;
  HRESULT STDMETHODCALLTYPE get_TouchConfidence(boolean*) override;
  HRESULT STDMETHODCALLTYPE get_IsLeftButtonPressed(boolean*) override;
  HRESULT STDMETHODCALLTYPE get_IsRightButtonPressed(boolean*) override;
  HRESULT STDMETHODCALLTYPE get_IsMiddleButtonPressed(boolean*) override;
  HRESULT STDMETHODCALLTYPE get_MouseWheelDelta(INT32*) override;
  HRESULT STDMETHODCALLTYPE get_IsHorizontalMouseWheel(boolean*) override;
  HRESULT STDMETHODCALLTYPE get_IsPrimary(boolean*) override;
  HRESULT STDMETHODCALLTYPE get_IsInRange(boolean*) override;
  HRESULT STDMETHODCALLTYPE get_IsCanceled(boolean*) override;
  HRESULT STDMETHODCALLTYPE get_IsBarrelButtonPressed(boolean*) override;
  HRESULT STDMETHODCALLTYPE get_IsXButton1Pressed(boolean*) override;
  HRESULT STDMETHODCALLTYPE get_IsXButton2Pressed(boolean*) override;
  HRESULT STDMETHODCALLTYPE
  get_PointerUpdateKind(ABI::Windows::UI::Input::PointerUpdateKind*) override;
  HRESULT STDMETHODCALLTYPE HasUsage(UINT32, UINT32, boolean*) override;
  HRESULT STDMETHODCALLTYPE GetUsageValue(UINT32, UINT32, INT32*) override;

  int tsn() { return tsn_; }
  int tvid() { return tvid_; }

 private:
  bool has_usage_throws_error_ = false;
  bool get_usage_throws_error_ = false;
  int tsn_;
  int tvid_;
};

}  // namespace views

#endif  //    UI_VIEWS_WIN_TEST_SUPPORT_FAKE_IPOINTER_POINT_PROPERTIES_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_TEST_SUPPORT_FAKE_IPEN_POINTER_POINT_STATICS_H_
#define UI_VIEWS_WIN_TEST_SUPPORT_FAKE_IPEN_POINTER_POINT_STATICS_H_

#include <windows.devices.input.h>
#include <windows.foundation.collections.h>
#include <windows.ui.input.h>
#include <wrl.h>

#include <unordered_map>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/core_winrt_util.h"
#include "base/win/hstring_reference.h"
#include "base/win/win_util.h"

namespace views {

using ABI::Windows::UI::Input::IPointerPoint;
using ABI::Windows::UI::Input::IPointerPointStatics;
using ABI::Windows::UI::Input::IPointerPointTransform;
using ABI::Windows::UI::Input::PointerPoint;

// ABI::Windows::UI::Input::IPointerPointStatics fake implementation.
class FakeIPenPointerPointStatics final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          IPointerPointStatics> {
 public:
  FakeIPenPointerPointStatics();

  FakeIPenPointerPointStatics(const FakeIPenPointerPointStatics&) = delete;
  FakeIPenPointerPointStatics& operator=(const FakeIPenPointerPointStatics&) =
      delete;

  ~FakeIPenPointerPointStatics() final;

  static FakeIPenPointerPointStatics* GetInstance();
  static Microsoft::WRL::ComPtr<IPointerPointStatics>
  FakeIPenPointerPointStaticsComPtr();

  HRESULT WINAPI GetCurrentPoint(UINT32 pointer_id,
                                 IPointerPoint** pointer_point) override;

  HRESULT STDMETHODCALLTYPE GetIntermediatePoints(
      UINT32 pointer_id,
      ABI::Windows::Foundation::Collections::IVector<PointerPoint*>** points)
      override;
  HRESULT STDMETHODCALLTYPE
  GetCurrentPointTransformed(UINT32 pointer_id,
                             IPointerPointTransform* t,
                             IPointerPoint**) override;
  HRESULT STDMETHODCALLTYPE GetIntermediatePointsTransformed(
      UINT32 pointer_id,
      IPointerPointTransform* t,
      ABI::Windows::Foundation::Collections::IVector<PointerPoint*>** points)
      override;

  // Test methods
  void AddPointerPoint(UINT32 pointer_id,
                       Microsoft::WRL::ComPtr<IPointerPoint> pointer_point);
  void ClearPointerPointsMap();

 private:
  std::unordered_map<
      /*pointer_id=*/UINT32,
      Microsoft::WRL::ComPtr<IPointerPoint>>
      pointer_point_map_;
};

}  // namespace views

#endif  //  UI_VIEWS_WIN_TEST_SUPPORT_FAKE_IPEN_POINTER_POINT_STATICS_H_

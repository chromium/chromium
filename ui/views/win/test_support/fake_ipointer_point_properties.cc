// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/test_support/fake_ipointer_point_properties.h"

#include <combaseapi.h>
#include <wchar.h>

#include <algorithm>
#include <string>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"

using Microsoft::WRL::ComPtr;

namespace views {

#define HID_USAGE_ID_TSN ((UINT)0x5b)
#define HID_USAGE_ID_TVID ((UINT)0x91)

FakeIPointerPointProperties::FakeIPointerPointProperties() = default;
FakeIPointerPointProperties::FakeIPointerPointProperties(
    bool has_usage_throws_error,
    bool get_usage_throws_error,
    int tsn,
    int tvid)
    : has_usage_throws_error_(has_usage_throws_error),
      get_usage_throws_error_(get_usage_throws_error),
      tsn_(tsn),
      tvid_(tvid) {}

FakeIPointerPointProperties::~FakeIPointerPointProperties() = default;

HRESULT FakeIPointerPointProperties::get_IsInverted(boolean*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_Pressure(float*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_IsEraser(boolean*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_Orientation(float*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_XTilt(float*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_YTilt(float*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_Twist(float*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_ContactRect(
    ABI::Windows::Foundation::Rect*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_ContactRectRaw(
    ABI::Windows::Foundation::Rect*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_TouchConfidence(boolean*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_IsLeftButtonPressed(boolean*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_IsRightButtonPressed(boolean*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_IsMiddleButtonPressed(boolean*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_MouseWheelDelta(INT32*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_IsHorizontalMouseWheel(boolean*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_IsPrimary(boolean*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_IsInRange(boolean*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_IsCanceled(boolean*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_IsBarrelButtonPressed(boolean*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_IsXButton1Pressed(boolean*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_IsXButton2Pressed(boolean*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::get_PointerUpdateKind(
    ABI::Windows::UI::Input::PointerUpdateKind*) {
  NOTIMPLEMENTED();
  return S_OK;
}
HRESULT FakeIPointerPointProperties::HasUsage(UINT32, UINT32, boolean* value) {
  if (has_usage_throws_error_) {
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  }

  *value = true;
  return S_OK;
}
HRESULT FakeIPointerPointProperties::GetUsageValue(UINT32 a,
                                                   UINT32 id_type,
                                                   INT32* value) {
  if (get_usage_throws_error_) {
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  }

  if (id_type == HID_USAGE_ID_TSN) {
    *value = tsn();
  } else if (id_type == HID_USAGE_ID_TVID) {
    *value = tvid();
  } else {
    NOTREACHED();
  }
  return S_OK;
}

}  // namespace views

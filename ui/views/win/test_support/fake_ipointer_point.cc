// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/test_support/fake_ipointer_point.h"

#include <combaseapi.h>
#include <wchar.h>

#include <algorithm>
#include <string>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "ui/views/win/test_support/fake_ipointer_point_properties.h"

using Microsoft::WRL::ComPtr;

namespace views {

FakeIPointerPoint::FakeIPointerPoint()
    : properties_(Microsoft::WRL::Make<FakeIPointerPointProperties>()) {}

FakeIPointerPoint::FakeIPointerPoint(bool throw_error_in_get_properties,
                                     bool has_usage_throws_error,
                                     bool get_usage_throws_error,
                                     int tsn,
                                     int tvid)
    : throw_error_in_get_properties_(throw_error_in_get_properties),
      properties_(Microsoft::WRL::Make<FakeIPointerPointProperties>(
          has_usage_throws_error,
          get_usage_throws_error,
          tsn,
          tvid)) {}

FakeIPointerPoint::~FakeIPointerPoint() = default;

HRESULT FakeIPointerPoint::get_Properties(
    ABI::Windows::UI::Input::IPointerPointProperties** value) {
  if (throw_error_in_get_properties_) {
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  }

  properties_.CopyTo(value);
  return S_OK;
}

HRESULT WINAPI FakeIPointerPoint::get_PointerDevice(
    ABI::Windows::Devices::Input::IPointerDevice** value) {
  NOTIMPLEMENTED();
  return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}
HRESULT WINAPI
FakeIPointerPoint::get_Position(ABI::Windows::Foundation::Point* value) {
  NOTIMPLEMENTED();
  return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}
HRESULT WINAPI
FakeIPointerPoint::get_RawPosition(ABI::Windows::Foundation::Point* value) {
  NOTIMPLEMENTED();
  return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}
HRESULT WINAPI FakeIPointerPoint::get_PointerId(UINT32* value) {
  NOTIMPLEMENTED();
  return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}
HRESULT WINAPI FakeIPointerPoint::get_FrameId(UINT32* value) {
  NOTIMPLEMENTED();
  return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}
HRESULT WINAPI FakeIPointerPoint::get_Timestamp(UINT64* value) {
  NOTIMPLEMENTED();
  return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}
HRESULT WINAPI FakeIPointerPoint::get_IsInContact(boolean* value) {
  NOTIMPLEMENTED();
  return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}

}  // namespace views

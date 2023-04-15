// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/test_support/fake_ipen_pointer_point_statics.h"

#include "base/no_destructor.h"
#include "base/notreached.h"

namespace views {

FakeIPenPointerPointStatics::FakeIPenPointerPointStatics() = default;
FakeIPenPointerPointStatics::~FakeIPenPointerPointStatics() = default;

// static
FakeIPenPointerPointStatics* FakeIPenPointerPointStatics::GetInstance() {
  // This instantiation contributes to singleton lazy initialization.
  static base::NoDestructor<FakeIPenPointerPointStatics> instance;
  return instance.get();
}

// static
Microsoft::WRL::ComPtr<IPointerPointStatics>
FakeIPenPointerPointStatics::FakeIPenPointerPointStaticsComPtr() {
  FakeIPenPointerPointStatics* instance = GetInstance();
  return static_cast<Microsoft::WRL::ComPtr<IPointerPointStatics>>(instance);
}

HRESULT FakeIPenPointerPointStatics::GetCurrentPoint(
    UINT32 pointer_id,
    ABI::Windows::UI::Input::IPointerPoint** result) {
  auto pointer_point = pointer_point_map_.find(pointer_id);
  if (pointer_point == pointer_point_map_.end()) {
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  }

  // This call always return S_OK.
  return pointer_point->second.CopyTo(result);
}

void FakeIPenPointerPointStatics::AddPointerPoint(
    UINT32 pointer_id,
    Microsoft::WRL::ComPtr<ABI::Windows::UI::Input::IPointerPoint>
        pointer_point) {
  pointer_point_map_[pointer_id] = pointer_point;
}

void FakeIPenPointerPointStatics::ClearPointerPointsMap() {
  pointer_point_map_.clear();
}

HRESULT FakeIPenPointerPointStatics::GetIntermediatePoints(
    UINT32 pointer_id,
    ABI::Windows::Foundation::Collections::IVector<
        ABI::Windows::UI::Input::PointerPoint*>** points) {
  NOTIMPLEMENTED();
  return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}
HRESULT FakeIPenPointerPointStatics::GetCurrentPointTransformed(
    UINT32 pointer_id,
    ABI::Windows::UI::Input::IPointerPointTransform* t,
    ABI::Windows::UI::Input::IPointerPoint** p) {
  NOTIMPLEMENTED();
  return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}
HRESULT FakeIPenPointerPointStatics::GetIntermediatePointsTransformed(
    UINT32 pointer_id,
    ABI::Windows::UI::Input::IPointerPointTransform* t,
    ABI::Windows::Foundation::Collections::IVector<
        ABI::Windows::UI::Input::PointerPoint*>** points) {
  NOTIMPLEMENTED();
  return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}

}  // namespace views

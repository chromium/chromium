// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/test_support/fake_ipen_device.h"

#include <combaseapi.h>

#include <string>

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"

namespace views {

FakeIPenDevice::FakeIPenDevice() {
  // Initialize guid_ with a random GUID.
  CHECK_EQ(CoCreateGuid(&guid_), S_OK);
}

FakeIPenDevice::~FakeIPenDevice() = default;

HRESULT FakeIPenDevice::get_PenId(GUID* value) {
  *value = guid_;
  return S_OK;
}

std::string FakeIPenDevice::GetGuid() const {
  return base::WideToUTF8(base::win::WStringFromGUID(guid_));
}

}  // namespace views

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/pen_id_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/hstring_reference.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"

namespace views {

namespace {

bool PenDeviceApiSupported() {
  // PenDevice API only works properly on WIN11 or Win10 post v19044.
  return base::win::GetVersion() > base::win::Version::WIN10_21H2 ||
         (base::win::GetVersion() == base::win::Version::WIN10_21H2 &&
          base::win::OSInfo::GetInstance()->version_number().patch >= 1503);
}

}  // namespace

PenIdHandler::PenIdHandler() {
  base::win::AssertComInitialized();
  HRESULT hr = base::win::RoGetActivationFactory(
      base::win::HStringReference(RuntimeClass_Windows_Devices_Input_PenDevice)
          .Get(),
      IID_PPV_ARGS(&pen_device_statics_));
  if (FAILED(hr)) {
    pen_device_statics_ = nullptr;
  }
}

PenIdHandler::~PenIdHandler() = default;

absl::optional<int32_t> PenIdHandler::TryGetPenUniqueId(UINT32 pointer_id) {
  if (!PenDeviceApiSupported()) {
    return absl::nullopt;
  }

  absl::optional<std::string> guid = TryGetGuid(pointer_id);
  if (!guid.has_value()) {
    return absl::nullopt;
  }

  auto entry = guid_to_id_map_.insert({guid.value(), current_id_});
  if (entry.second) {
    current_id_++;
  }
  return entry.first->second;
}

absl::optional<std::string> PenIdHandler::TryGetGuid(UINT32 pointer_id) const {
  if (!pen_device_statics_) {
    return absl::nullopt;
  }

  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Input::IPenDevice> pen_device;
  HRESULT hr = pen_device_statics_->GetFromPointerId(pointer_id, &pen_device);
  // `pen_device` is null if the pen does not support a unique ID.
  if (FAILED(hr) || !pen_device) {
    return absl::nullopt;
  }

  GUID pen_device_guid;
  hr = pen_device->get_PenId(&pen_device_guid);
  if (FAILED(hr)) {
    return absl::nullopt;
  }

  return base::WideToUTF8(base::win::WStringFromGUID(pen_device_guid));
}

}  // namespace views

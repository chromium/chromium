// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/pen_id_handler.h"

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/hstring_reference.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"

namespace views {

namespace {

using ABI::Windows::Devices::Input::IPenDevice;
using ABI::Windows::Devices::Input::IPenDeviceStatics;
using ABI::Windows::UI::Input::IPointerPoint;
using ABI::Windows::UI::Input::IPointerPointProperties;
using ABI::Windows::UI::Input::IPointerPointStatics;
using Microsoft::WRL::ComPtr;

#define HID_USAGE_PAGE_DIGITIZER ((UINT)0x0d)
#define HID_USAGE_ID_TSN ((UINT)0x5b)
#define HID_USAGE_ID_TVID ((UINT)0x91)

PenIdHandler::GetPenDeviceStatics get_pen_device_statics = nullptr;
PenIdHandler::GetPointerPointStatics get_pointer_point_statics = nullptr;

class PenIdStatics {
 public:
  PenIdStatics() {
    if (skip_initialization_) {
      return;
    }
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    base::win::AssertComInitialized();
    base::win::RoGetActivationFactory(
        base::win::HStringReference(
            RuntimeClass_Windows_Devices_Input_PenDevice)
            .Get(),
        IID_PPV_ARGS(&pen_device_statics_));

    base::win::RoGetActivationFactory(
        base::win::HStringReference(RuntimeClass_Windows_UI_Input_PointerPoint)
            .Get(),
        IID_PPV_ARGS(&pointer_point_statics_));
  }

  static PenIdStatics* GetInstance() {
    static base::NoDestructor<PenIdStatics> instance;
    return instance.get();
  }

  static void SkipInitializationForTesting() {
    if (skip_initialization_) {
      return;
    }

    skip_initialization_ = true;

    // Force PenIdStatics to skip initialization.
    PenIdStatics::GetInstance();

    // Check that initialization hasn't already occurred.
    DCHECK_EQ(nullptr, PenIdStatics::GetInstance()->PenDeviceStatics());
    DCHECK_EQ(nullptr, PenIdStatics::GetInstance()->PointerPointStatics());
  }

  const ComPtr<IPenDeviceStatics> PenDeviceStatics() {
    return pen_device_statics_;
  }

  const ComPtr<IPointerPointStatics> PointerPointStatics() {
    return pointer_point_statics_;
  }

 private:
  ComPtr<IPenDeviceStatics> pen_device_statics_;
  ComPtr<IPointerPointStatics> pointer_point_statics_;
  static bool skip_initialization_;
};

// Declared private in PenIdStatics.
bool PenIdStatics::skip_initialization_ = false;

bool PenDeviceApiSupported() {
  // PenDevice API only works properly on WIN11 or Win10 post v19044.
  return base::win::OSInfo::Kernel32Version() >
             base::win::Version::WIN10_21H2 ||
         (base::win::OSInfo::Kernel32Version() ==
              base::win::Version::WIN10_21H2 &&
          base::win::OSInfo::GetInstance()->version_number().patch >= 1503);
}

}  // namespace

PenIdHandler::ScopedPenIdStaticsForTesting::ScopedPenIdStaticsForTesting(
    PenIdHandler::GetPenDeviceStatics pen_device_statics,
    PenIdHandler::GetPointerPointStatics pointer_point_statics)
    : pen_device_resetter_(&get_pen_device_statics, pen_device_statics),
      pointer_point_resetter_(&get_pointer_point_statics,
                              pointer_point_statics) {
  PenIdStatics::SkipInitializationForTesting();
}
PenIdHandler::ScopedPenIdStaticsForTesting::~ScopedPenIdStaticsForTesting() =
    default;

PenIdHandler::PenIdHandler() {
  InitPenIdStatics();
}

PenIdHandler::~PenIdHandler() = default;

absl::optional<int32_t> PenIdHandler::TryGetPenUniqueId(UINT32 pointer_id) {
  if (!PenDeviceApiSupported()) {
    return absl::nullopt;
  }

  absl::optional<std::string> guid = TryGetGuid(pointer_id);
  if (guid.has_value()) {
    auto entry = guid_to_id_map_.insert({guid.value(), current_id_});
    if (entry.second) {
      current_id_++;
    }
    return entry.first->second;
  }

  PenIdHandler::TransducerId transducer_id = TryGetTransducerId(pointer_id);
  if (transducer_id.tsn != TransducerId::kInvalidTSN) {
    if (!transducer_id_to_id_map_.contains(transducer_id)) {
      transducer_id_to_id_map_[transducer_id] = current_id_++;
    }
    return transducer_id_to_id_map_[transducer_id];
  }

  return absl::nullopt;
}

absl::optional<std::string> PenIdHandler::TryGetGuid(UINT32 pointer_id) const {
  // Override pen device statics if in a test.
  const Microsoft::WRL::ComPtr<IPenDeviceStatics> pen_device_statics =
      get_pen_device_statics ? (*get_pen_device_statics)()
                             : PenIdStatics::GetInstance()->PenDeviceStatics();

  // Return absl::nullopt if we are not in a testing environment and the
  // pen device statics haven't loaded or if statics are null.
  if (!pen_device_statics) {
    return absl::nullopt;
  }

  Microsoft::WRL::ComPtr<IPenDevice> pen_device;
  HRESULT hr = pen_device_statics->GetFromPointerId(pointer_id, &pen_device);
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

PenIdHandler::TransducerId PenIdHandler::TryGetTransducerId(
    UINT32 pointer_id) const {
  // Override pointer point statics if in a test.
  const Microsoft::WRL::ComPtr<IPointerPointStatics> pointer_point_statics =
      get_pointer_point_statics
          ? (*get_pointer_point_statics)()
          : PenIdStatics::GetInstance()->PointerPointStatics();

  TransducerId transducer_id;
  if (!pointer_point_statics) {
    return transducer_id;
  }

  ComPtr<IPointerPoint> pointer_point;
  HRESULT hr =
      pointer_point_statics->GetCurrentPoint(pointer_id, &pointer_point);
  if (hr != S_OK) {
    return transducer_id;
  }

  ComPtr<IPointerPointProperties> pointer_point_properties;
  hr = pointer_point->get_Properties(&pointer_point_properties);
  if (hr != S_OK) {
    return transducer_id;
  }

  // Retrieve Transducer Serial Number and check if it's valid.
  boolean has_tsn = false;
  hr = pointer_point_properties->HasUsage(HID_USAGE_PAGE_DIGITIZER,
                                          HID_USAGE_ID_TSN, &has_tsn);

  if (hr != S_OK || !has_tsn) {
    return transducer_id;
  }

  hr = pointer_point_properties->GetUsageValue(
      HID_USAGE_PAGE_DIGITIZER, HID_USAGE_ID_TSN, &transducer_id.tsn);

  if (hr != S_OK || transducer_id.tsn == TransducerId::kInvalidTSN) {
    return transducer_id;
  }

  // Retrieve Transducer Vendor Id and check if it's valid.
  boolean has_tvid = false;
  hr = pointer_point_properties->HasUsage(HID_USAGE_PAGE_DIGITIZER,
                                          HID_USAGE_ID_TVID, &has_tvid);

  if (hr != S_OK || !has_tvid) {
    return transducer_id;
  }

  hr = pointer_point_properties->GetUsageValue(
      HID_USAGE_PAGE_DIGITIZER, HID_USAGE_ID_TVID, &transducer_id.tvid);

  return transducer_id;
}

void PenIdHandler::InitPenIdStatics() {
  static bool initialized = false;
  if (initialized) {
    return;
  }

  initialized = true;

  // Initialize the statics by creating a static instance of PenIdStatics. This
  // is done in an worker thread to avoid jank during startup. If the statics
  // are not initialized by the time they are needed, use of
  // PenIdStatics::GetInstance will be a blocking call until they are
  // loaded.
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&PenIdStatics::GetInstance)));
}

}  // namespace views

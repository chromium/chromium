// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/pen_id_handler.h"

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/trace_event/trace_event.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/hstring_reference.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"

namespace views {

namespace {

using ABI::Windows::Devices::Input::IPenDevice;
using ABI::Windows::Devices::Input::IPenDeviceStatics;
using Microsoft::WRL::ComPtr;

#define HID_USAGE_PAGE_DIGITIZER ((UINT)0x0d)
#define HID_USAGE_ID_TSN ((UINT)0x5b)
#define HID_USAGE_ID_TVID ((UINT)0x91)

PenIdHandler::GetPenDeviceStatics get_pen_device_statics = nullptr;

class PenIdStatics {
 public:
  PenIdStatics() {
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    base::win::AssertComInitialized();
    base::win::RoGetActivationFactory(
        base::win::HStringReference(
            RuntimeClass_Windows_Devices_Input_PenDevice)
            .Get(),
        IID_PPV_ARGS(&pen_device_statics_));
    TRACE_EVENT_INSTANT0("event", "PenIdStatics::PenIdStatics",
                         TRACE_EVENT_SCOPE_THREAD);
  }

  static PenIdStatics* GetInstance() {
    static base::NoDestructor<PenIdStatics> instance;
    return instance.get();
  }

  const ComPtr<IPenDeviceStatics> PenDeviceStatics() {
    return pen_device_statics_;
  }

 private:
  ComPtr<IPenDeviceStatics> pen_device_statics_;
};

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
    PenIdHandler::GetPenDeviceStatics pen_device_statics)
    : pen_device_resetter_(&get_pen_device_statics, pen_device_statics) {}

PenIdHandler::ScopedPenIdStaticsForTesting::~ScopedPenIdStaticsForTesting() =
    default;

PenIdHandler::PenIdHandler() {
  InitPenIdStatics();
}

PenIdHandler::~PenIdHandler() = default;

std::optional<int32_t> PenIdHandler::TryGetPenUniqueId(UINT32 pointer_id) {
  if (!PenDeviceApiSupported()) {
    return std::nullopt;
  }

  std::optional<std::string> guid = TryGetGuid(pointer_id);
  if (guid.has_value()) {
    auto entry = guid_to_id_map_.insert({guid.value(), current_id_});
    if (entry.second) {
      current_id_++;
    }
    return entry.first->second;
  }

  return std::nullopt;
}

std::optional<std::string> PenIdHandler::TryGetGuid(UINT32 pointer_id) const {
  // Override pen device statics if in a test.
  const Microsoft::WRL::ComPtr<IPenDeviceStatics> pen_device_statics =
      get_pen_device_statics ? (*get_pen_device_statics)()
                             : PenIdStatics::GetInstance()->PenDeviceStatics();

  // Return std::nullopt if we are not in a testing environment and the
  // pen device statics haven't loaded or if statics are null.
  if (!pen_device_statics) {
    TRACE_EVENT_INSTANT0("event", "PenIdHandler::TryGetGuid no statics",
                         TRACE_EVENT_SCOPE_THREAD);
    return std::nullopt;
  }

  Microsoft::WRL::ComPtr<IPenDevice> pen_device;
  HRESULT hr = pen_device_statics->GetFromPointerId(pointer_id, &pen_device);
  // `pen_device` is null if the pen does not support a unique ID.
  if (FAILED(hr) || !pen_device) {
    TRACE_EVENT_INSTANT0("event",
                         "PenIdHandler::TryGetGuid GetFromPointerId failed",
                         TRACE_EVENT_SCOPE_THREAD);
    return std::nullopt;
  }

  GUID pen_device_guid;
  hr = pen_device->get_PenId(&pen_device_guid);
  if (FAILED(hr)) {
    TRACE_EVENT_INSTANT0("event", "PenIdHandler::TryGetGuid get_PenId failed",
                         TRACE_EVENT_SCOPE_THREAD);
    return std::nullopt;
  }

  TRACE_EVENT_INSTANT0("event", "PenIdHandler::TryGetGuid successful",
                       TRACE_EVENT_SCOPE_THREAD);
  return base::WideToUTF8(base::win::WStringFromGUID(pen_device_guid));
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

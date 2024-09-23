// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/color_profile_reader.h"

#include <windows.h>

#include <stddef.h>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_thread_priority.h"
#include "ui/display/win/display_info.h"
#include "ui/gfx/icc_profile.h"

namespace display {
namespace win {
namespace {

BOOL CALLBACK EnumMonitorForProfilePathCallback(HMONITOR monitor,
                                                HDC input_hdc,
                                                LPRECT rect,
                                                LPARAM data) {
  std::wstring device_name;
  MONITORINFOEX monitor_info;
  ::ZeroMemory(&monitor_info, sizeof(monitor_info));
  monitor_info.cbSize = sizeof(monitor_info);
  ::GetMonitorInfo(monitor, &monitor_info);
  device_name = std::wstring(monitor_info.szDevice);

  std::wstring profile_path;
  HDC hdc = ::CreateDC(monitor_info.szDevice, NULL, NULL, NULL);
  if (hdc) {
    DWORD path_length = MAX_PATH;
    WCHAR path[MAX_PATH + 1];
    BOOL result = ::GetICMProfile(hdc, &path_length, path);
    ::DeleteDC(hdc);
    if (result)
      profile_path = std::wstring(path);
  }
  int64_t display_id =
      internal::DisplayInfo::DisplayIdFromMonitorInfo(monitor_info);
  std::map<int64_t, std::wstring>* display_id_to_path_map =
      reinterpret_cast<std::map<int64_t, std::wstring>*>(data);
  (*display_id_to_path_map)[display_id] = profile_path;
  return TRUE;
}

}  // namespace

ColorProfileReader::ColorProfileReader(Client* client) : client_(client) {}

ColorProfileReader::~ColorProfileReader() {}

void ColorProfileReader::UpdateIfNeeded() {
  // There is a potential race condition wherein the result from
  // EnumDisplayMonitors is already stale by the time that we get
  // back to BuildDisplayIdToPathMapCompleted.  To fix this we would
  // need to record the fact that we early-out-ed because of
  // update_in_flight_ was true, and then re-issue
  // BuildDisplayIdToPathMapOnBackgroundThread when the update
  // returned.
  if (update_in_flight_)
    return;

  update_in_flight_ = true;

  // Enumerate device profile paths on a background thread.  When this
  // completes it will run another task on a background thread to read
  // the profiles. This can impact the color of the browser so we want
  // to set this to a higher priority to complete the task earlier
  // during startup.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          &ColorProfileReader::BuildDisplayIdToPathMapOnBackgroundThread),
      base::BindOnce(&ColorProfileReader::BuildDisplayIdToPathMapCompleted,
                     weak_factory_.GetWeakPtr()));
}

// static
ColorProfileReader::DisplayIdToPathMap
ColorProfileReader::BuildDisplayIdToPathMapOnBackgroundThread() {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
  DisplayIdToPathMap display_id_to_path_map;
  EnumDisplayMonitors(nullptr, nullptr, EnumMonitorForProfilePathCallback,
                      reinterpret_cast<LPARAM>(&display_id_to_path_map));
  return display_id_to_path_map;
}

void ColorProfileReader::BuildDisplayIdToPathMapCompleted(
    DisplayIdToPathMap new_display_id_to_path_map) {
  DCHECK(update_in_flight_);

  // Are there any changes from previous results
  if (display_id_to_path_map_ == new_display_id_to_path_map) {
    update_in_flight_ = false;
    return;
  }

  display_id_to_path_map_ = new_display_id_to_path_map;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ColorProfileReader::ReadProfilesOnBackgroundThread,
                     new_display_id_to_path_map),
      base::BindOnce(&ColorProfileReader::ReadProfilesCompleted,
                     weak_factory_.GetWeakPtr()));
}

// static
ColorProfileReader::DisplayIdToDataMap
ColorProfileReader::ReadProfilesOnBackgroundThread(
    DisplayIdToPathMap new_display_id_to_path_map) {
  DisplayIdToDataMap new_display_id_to_data_map;
  for (const auto& [display_id, profile_path] : new_display_id_to_path_map) {
    std::string profile_data;
    base::ReadFileToString(base::FilePath(profile_path), &profile_data);
    new_display_id_to_data_map[display_id] = profile_data;
  }
  return new_display_id_to_data_map;
}

void ColorProfileReader::ReadProfilesCompleted(
    DisplayIdToDataMap display_id_to_data_map) {
  DCHECK(update_in_flight_);
  update_in_flight_ = false;

  display_id_to_profile_map_.clear();
  for (const auto& [display_id, profile_data] : display_id_to_data_map) {
    if (!profile_data.empty()) {
      display_id_to_profile_map_[display_id] =
          gfx::ICCProfile::FromData(profile_data.data(), profile_data.size());
    }
  }

  client_->OnColorProfilesChanged();
}

gfx::ColorSpace ColorProfileReader::GetDisplayColorSpace(
    int64_t display_id) const {
  gfx::ICCProfile icc_profile;
  auto found = display_id_to_profile_map_.find(display_id);
  if (found != display_id_to_profile_map_.end())
    icc_profile = found->second;
  return icc_profile.IsValid() ? icc_profile.GetPrimariesOnlyColorSpace()
                               : gfx::ColorSpace::CreateSRGB();
}

}  // namespace win
}  // namespace display

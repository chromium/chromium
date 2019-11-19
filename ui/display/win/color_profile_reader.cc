// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/color_profile_reader.h"

#include <stddef.h>
#include <windows.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "ui/display/win/display_info.h"
#include "ui/gfx/icc_profile.h"

namespace display {
namespace win {
namespace {

BOOL CALLBACK EnumMonitorForProfilePathCallback(HMONITOR monitor,
                                                HDC input_hdc,
                                                LPRECT rect,
                                                LPARAM data) {
  base::string16 device_name;
  MONITORINFOEX monitor_info;
  ::ZeroMemory(&monitor_info, sizeof(monitor_info));
  monitor_info.cbSize = sizeof(monitor_info);
  ::GetMonitorInfo(monitor, &monitor_info);
  device_name = base::string16(monitor_info.szDevice);

  base::string16 profile_path;
  HDC hdc = ::CreateDC(monitor_info.szDevice, NULL, NULL, NULL);
  if (hdc) {
    DWORD path_length = MAX_PATH;
    WCHAR path[MAX_PATH + 1];
    BOOL result = ::GetICMProfile(hdc, &path_length, path);
    ::DeleteDC(hdc);
    if (result)
      profile_path = base::string16(path);
  }

  std::map<base::string16, base::string16>* device_to_path_map =
      reinterpret_cast<std::map<base::string16, base::string16>*>(data);
  (*device_to_path_map)[device_name] = profile_path;
  return TRUE;
}

}  // namespace

ColorProfileReader::ColorProfileReader(Client* client) : client_(client) {}

ColorProfileReader::~ColorProfileReader() {}

void ColorProfileReader::UpdateIfNeeded() {
  // There is a potential race condition wherein the result from
  // EnumDisplayMonitors is already stale by the time that we get
  // back to BuildDeviceToPathMapCompleted.  To fix this we would
  // need to record the fact that we early-out-ed because of
  // update_in_flight_ was true, and then re-issue
  // BuildDeviceToPathMapOnBackgroundThread when the update
  // returned.
  if (update_in_flight_)
    return;

  update_in_flight_ = true;

  // Enumerate device profile paths on a background thread.  When this
  // completes it will run another task on a background thread to read
  // the profiles.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&ColorProfileReader::BuildDeviceToPathMapOnBackgroundThread),
      base::Bind(&ColorProfileReader::BuildDeviceToPathMapCompleted,
                 weak_factory_.GetWeakPtr()));
}

// static
ColorProfileReader::DeviceToPathMap
ColorProfileReader::BuildDeviceToPathMapOnBackgroundThread() {
  DeviceToPathMap device_to_path_map;
  EnumDisplayMonitors(nullptr, nullptr, EnumMonitorForProfilePathCallback,
                      reinterpret_cast<LPARAM>(&device_to_path_map));
  return device_to_path_map;
}

void ColorProfileReader::BuildDeviceToPathMapCompleted(
    DeviceToPathMap new_device_to_path_map) {
  DCHECK(update_in_flight_);

  // Are there any changes from previous results
  if (device_to_path_map_ == new_device_to_path_map) {
    update_in_flight_ = false;
    return;
  }

  device_to_path_map_ = new_device_to_path_map;

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&ColorProfileReader::ReadProfilesOnBackgroundThread,
                 new_device_to_path_map),
      base::Bind(&ColorProfileReader::ReadProfilesCompleted,
                 weak_factory_.GetWeakPtr()));
}

// static
ColorProfileReader::DeviceToDataMap
ColorProfileReader::ReadProfilesOnBackgroundThread(
    DeviceToPathMap new_device_to_path_map) {
  DeviceToDataMap new_device_to_data_map;
  for (auto entry : new_device_to_path_map) {
    const base::string16& device_name = entry.first;
    const base::string16& profile_path = entry.second;
    std::string profile_data;
    base::ReadFileToString(base::FilePath(profile_path), &profile_data);
    new_device_to_data_map[device_name] = profile_data;
  }
  return new_device_to_data_map;
}

void ColorProfileReader::ReadProfilesCompleted(
    DeviceToDataMap device_to_data_map) {
  DCHECK(update_in_flight_);
  update_in_flight_ = false;
  has_read_profiles_ = true;

  display_id_to_profile_map_.clear();
  for (auto entry : device_to_data_map) {
    const base::string16& device_name = entry.first;
    const std::string& profile_data = entry.second;
    if (!profile_data.empty()) {
      int64_t display_id =
          DisplayInfo::DeviceIdFromDeviceName(device_name.c_str());
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
  if (has_read_profiles_)
    icc_profile.HistogramDisplay(display_id);
  return icc_profile.IsValid() ? icc_profile.GetPrimariesOnlyColorSpace()
                               : gfx::ColorSpace::CreateSRGB();
}

}  // namespace win
}  // namespace display

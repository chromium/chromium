// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/test/virtual_display_win_util.h"

#include "base/containers/flat_tree.h"
#include "base/logging.h"
#include "third_party/win_virtual_display/driver/public/properties.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/win/display_config_helper.h"
#include "ui/display/win/screen_win.h"

namespace display::test {

struct DisplayParams {
  explicit DisplayParams(MonitorConfig config) : monitor_config(config) {}
  MonitorConfig monitor_config;
};

VirtualDisplayWinUtil::VirtualDisplayWinUtil(Screen* screen) : screen_(screen) {
  screen_->AddObserver(this);
}

VirtualDisplayWinUtil::~VirtualDisplayWinUtil() {
  RemoveAllDisplays();
  screen_->RemoveObserver(this);
}

bool VirtualDisplayWinUtil::IsAPIAvailable() {
  return DisplayDriverController::IsDriverInstalled();
}

int64_t VirtualDisplayWinUtil::AddDisplay(unsigned short id,
                                          const DisplayParams& display_params) {
  if (virtual_displays_.find(id) != virtual_displays_.end()) {
    LOG(ERROR) << "Duplicate virtual display ID added: " << id;
    return kInvalidDisplayId;
  }
  std::vector<MonitorConfig> monitors;
  if (current_config_.has_value()) {
    monitors = current_config_->requested_configs();
  }
  MonitorConfig new_config = display_params.monitor_config;
  new_config.set_product_code(id);
  monitors.push_back(new_config);
  if (!SetDriverProperties(DriverProperties(monitors))) {
    return kInvalidDisplayId;
  }
  StartWaiting();
  auto created_display = virtual_displays_.find(id);
  CHECK(created_display != virtual_displays_.end());
  return created_display->second;
}

void VirtualDisplayWinUtil::RemoveDisplay(int64_t display_id) {
  auto it = std::find_if(
      virtual_displays_.begin(), virtual_displays_.end(),
      [&display_id](const std::pair<unsigned short, int64_t>& obj) {
        return obj.second == display_id;
      });
  if (it == virtual_displays_.end()) {
    LOG(WARNING) << "Display ID " << display_id << " is not a virtual display.";
    return;
  }
  std::vector<MonitorConfig> monitors = current_config_->requested_configs();
  std::erase_if(monitors, [&it](MonitorConfig& c) {
    return c.product_code() == it->first;
  });
  if (SetDriverProperties(DriverProperties(monitors))) {
    StartWaiting();
  }
}

void VirtualDisplayWinUtil::RemoveAllDisplays() {
  driver_controller_.Reset();
  if (current_config_.has_value()) {
    current_config_ = DriverProperties();
    StartWaiting();
  }
  virtual_displays_.clear();
  current_config_.reset();
}

void VirtualDisplayWinUtil::OnDisplayAdded(
    const display::Display& new_display) {
  if (!current_config_.has_value()) {
    return;
  }
  std::vector<MonitorConfig> requested = current_config_->requested_configs();
  HMONITOR monitor = ::MonitorFromPoint(
      win::ScreenWin::DIPToScreenPoint(new_display.work_area().CenterPoint())
          .ToPOINT(),
      MONITOR_DEFAULTTONEAREST);
  absl::optional<DISPLAYCONFIG_PATH_INFO> path_info =
      ::display::win::GetDisplayConfigPathInfo(monitor);
  if (::display::win::GetDisplayManufacturerId(path_info) ==
      kDriverMonitorManufacturer) {
    UINT16 product_code = ::display::win::GetDisplayProductCode(path_info);
    auto it = virtual_displays_.find(product_code);
    // Should never detect multiple displays with the same product code.
    CHECK(it == virtual_displays_.end())
        << "Detected duplicate virtual display product code:" << product_code;
    virtual_displays_[product_code] = new_display.id();
  }
  OnDisplayAddedOrRemoved(new_display.id());
}

void VirtualDisplayWinUtil::OnDisplayRemoved(
    const display::Display& old_display) {
  base::EraseIf(virtual_displays_,
                [&old_display](const std::pair<unsigned short, int64_t>& obj) {
                  return obj.second == old_display.id();
                });
  OnDisplayAddedOrRemoved(old_display.id());
}

bool VirtualDisplayWinUtil::SetDriverProperties(DriverProperties properties) {
  if (!driver_controller_.SetDisplayConfig(properties)) {
    LOG(ERROR) << "SetDisplayConfig failed: Failed to set display properties.";
    return false;
  }
  current_config_ = properties;
  return true;
}

void VirtualDisplayWinUtil::OnDisplayAddedOrRemoved(int64_t id) {
  if (!current_config_.has_value() ||
      virtual_displays_.size() != current_config_->requested_configs().size()) {
    return;
  }
  StopWaiting();
}

void VirtualDisplayWinUtil::StartWaiting() {
  DCHECK(!run_loop_);
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

void VirtualDisplayWinUtil::StopWaiting() {
  DCHECK(run_loop_);
  run_loop_->Quit();
}

const DisplayParams VirtualDisplayWinUtil::k1920x1080 =
    DisplayParams(MonitorConfig::k1920x1080);
const DisplayParams VirtualDisplayWinUtil::k1024x768 =
    DisplayParams(MonitorConfig::k1024x768);

}  // namespace display::test

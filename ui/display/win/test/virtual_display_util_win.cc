// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/test/virtual_display_util_win.h"

#include "base/containers/flat_tree.h"
#include "base/logging.h"
#include "third_party/win_virtual_display/driver/public/properties.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/win/display_config_helper.h"
#include "ui/display/win/screen_win.h"

namespace display::test {

namespace {

// Returns true if the current environment is detected to be headless.
bool IsHeadless() {
  DISPLAY_DEVICE adapter{};
  adapter.cb = sizeof(adapter);
  for (DWORD i = 0;
       EnumDisplayDevices(nullptr, i, &adapter, EDD_GET_DEVICE_INTERFACE_NAME);
       i++) {
    if (adapter.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
      // If any driver is considered attached, then not headless.
      // Note the default stub driver ("Microsoft Basic Display Driver") is
      // never considered "attached".
      return false;
    }
  }
  return true;
}

}  // namespace

struct DisplayParams {
  explicit DisplayParams(MonitorConfig config) : monitor_config(config) {}
  MonitorConfig monitor_config;
};

VirtualDisplayUtilWin::VirtualDisplayUtilWin(Screen* screen)
    : screen_(screen), is_headless_(IsHeadless()) {
  screen_->AddObserver(this);
  if (IsAPIAvailable()) {
    ResetDisplays();
  }
}

VirtualDisplayUtilWin::~VirtualDisplayUtilWin() {
  if (IsAPIAvailable()) {
    driver_controller_.Reset();
    if (virtual_displays_.size() > 0) {
      StartWaiting();
    }
  }
  screen_->RemoveObserver(this);
}

// static
bool VirtualDisplayUtilWin::IsAPIAvailable() {
  return DisplayDriverController::IsDriverInstalled();
}

int64_t VirtualDisplayUtilWin::AddDisplay(uint8_t id,
                                          const DisplayParams& display_params) {
  if (virtual_displays_.find(id) != virtual_displays_.end()) {
    LOG(ERROR) << "Duplicate virtual display ID added: " << id;
    return kInvalidDisplayId;
  }
  std::vector<MonitorConfig> monitors;
  monitors = current_config_.requested_configs();
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

void VirtualDisplayUtilWin::RemoveDisplay(int64_t display_id) {
  auto it = std::find_if(
      virtual_displays_.begin(), virtual_displays_.end(),
      [&display_id](const std::pair<unsigned short, int64_t>& obj) {
        return obj.second == display_id;
      });
  if (it == virtual_displays_.end()) {
    LOG(WARNING) << "Display ID " << display_id << " is not a virtual display.";
    return;
  }
  std::vector<MonitorConfig> monitors = current_config_.requested_configs();
  std::erase_if(monitors, [&it](MonitorConfig& c) {
    return c.product_code() == it->first;
  });
  if (SetDriverProperties(DriverProperties(monitors))) {
    StartWaiting();
  }
}

void VirtualDisplayUtilWin::ResetDisplays() {
  // Internal virtual display ID used for replacing a headless stub display.
  // This is arbitrarily chosen to be the max value that
  // MonitorConfig::set_product_code() supports.
  constexpr unsigned short kHeadlessDisplayId = 65535;
  DriverProperties prev_config = current_config_;
  DriverProperties new_config{};
  if (is_headless_) {
    LOG(INFO) << "Headless detected, setting base virtual display.";
    // In a headless environment, when no working display adapter is attached,
    // windows defaults to a pseudo/stub display. This causes the first call to
    // AddDisplay() to not add a display, but *replace* the stub with our
    // virtualized adapter. Therefore, we replace the default stub display with
    // our own virtual one. See:
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/display/support-for-headless-systems
    std::vector<MonitorConfig> configs{k1920x1080.monitor_config};
    configs[0].set_product_code(kHeadlessDisplayId);
    new_config = DriverProperties(configs);
  }
  SetDriverProperties(new_config);
  if (current_config_.requested_configs().size() !=
      prev_config.requested_configs().size()) {
    StartWaiting();
  }
}

void VirtualDisplayUtilWin::OnDisplayAdded(
    const display::Display& new_display) {
  std::vector<MonitorConfig> requested = current_config_.requested_configs();
  HMONITOR monitor = ::MonitorFromPoint(
      win::ScreenWin::DIPToScreenPoint(new_display.work_area().CenterPoint())
          .ToPOINT(),
      MONITOR_DEFAULTTONEAREST);
  std::optional<DISPLAYCONFIG_PATH_INFO> path_info =
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

void VirtualDisplayUtilWin::OnDisplayRemoved(
    const display::Display& old_display) {
  base::EraseIf(virtual_displays_,
                [&old_display](const std::pair<unsigned short, int64_t>& obj) {
                  return obj.second == old_display.id();
                });
  OnDisplayAddedOrRemoved(old_display.id());
}

bool VirtualDisplayUtilWin::SetDriverProperties(DriverProperties properties) {
  if (!driver_controller_.SetDisplayConfig(properties)) {
    LOG(ERROR) << "SetDisplayConfig failed: Failed to set display properties.";
    return false;
  }
  current_config_ = properties;
  return true;
}

void VirtualDisplayUtilWin::OnDisplayAddedOrRemoved(int64_t id) {
  if (virtual_displays_.size() != current_config_.requested_configs().size()) {
    return;
  }
  StopWaiting();
}

void VirtualDisplayUtilWin::StartWaiting() {
  CHECK(!run_loop_);
  run_loop_ = std::make_unique<base::RunLoop>();
  if (virtual_displays_.size() != current_config_.requested_configs().size()) {
    run_loop_->Run();
  }
  run_loop_.reset();
}

void VirtualDisplayUtilWin::StopWaiting() {
  CHECK(run_loop_);
  run_loop_->Quit();
}

const DisplayParams VirtualDisplayUtilWin::k1920x1080 =
    DisplayParams(MonitorConfig::k1920x1080);
const DisplayParams VirtualDisplayUtilWin::k1024x768 =
    DisplayParams(MonitorConfig::k1024x768);

// VirtualDisplayUtil definitions:
const DisplayParams VirtualDisplayUtil::k1920x1080 =
    VirtualDisplayUtilWin::k1920x1080;
const DisplayParams VirtualDisplayUtil::k1024x768 =
    VirtualDisplayUtilWin::k1024x768;

// static
std::unique_ptr<VirtualDisplayUtil> VirtualDisplayUtil::TryCreate(
    Screen* screen) {
  if (!VirtualDisplayUtilWin::IsAPIAvailable()) {
    return nullptr;
  }
  return std::make_unique<VirtualDisplayUtilWin>(screen);
}

}  // namespace display::test

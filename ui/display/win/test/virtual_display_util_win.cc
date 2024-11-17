// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/test/virtual_display_util_win.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <limits>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_tree.h"
#include "base/logging.h"
#include "third_party/win_virtual_display/driver/public/properties.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/virtual_display_util.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/win/display_config_helper.h"
#include "ui/display/win/screen_win.h"

namespace display::test {
namespace {

// Map of `VirtualDisplayUtil` configs to `MonitorConfig` defined in Windows
// driver controller code.
static const auto kConfigMap = base::MakeFixedFlatMap<
    display::test::DisplayParams,
    std::reference_wrapper<const display::test::MonitorConfig>>(
    {{display::test::VirtualDisplayUtil::k1024x768,
      display::test::MonitorConfig::k1024x768},
     {display::test::VirtualDisplayUtil::k1920x1080,
      display::test::MonitorConfig::k1920x1080}});

// Comparer for gfx:Size for use in sorting algorithms.
struct SizeCompare {
  bool operator()(const gfx::Size& a, const gfx::Size& b) const {
    if (a.width() == b.width()) {
      return a.height() < b.height();
    }
    return a.width() < b.width();
  }
};

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

// Sets the default display resolution to the specified size.
bool SetDisplayResolution(const gfx::Size& size) {
  DEVMODE mode;
  mode.dmSize = sizeof(DEVMODE);
  mode.dmPelsWidth = size.width();
  mode.dmPelsHeight = size.height();
  mode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
  LONG settings_result = ::ChangeDisplaySettings(&mode, 0);
  LOG_IF(ERROR, settings_result != DISP_CHANGE_SUCCESSFUL)
      << "ChangeDisplaySettings failed with result: " << settings_result;
  return settings_result == DISP_CHANGE_SUCCESSFUL;
}

// Creates a comma separated list of display strings (for debug /logs).
std::string JoinDisplayStrings(const std::vector<display::Display>& displays) {
  std::vector<std::string> parts;
  for (const auto& display : displays) {
    parts.push_back(display.ToString());
  }
  return base::JoinString(parts, ", ");
}

}  // namespace

VirtualDisplayUtilWin::VirtualDisplayUtilWin(Screen* screen)
    : screen_(screen), is_headless_(IsHeadless()) {
  screen_->AddObserver(this);
  initial_displays_ = screen_->GetAllDisplays();
  if (IsAPIAvailable()) {
    ResetDisplays();
  }
}

VirtualDisplayUtilWin::~VirtualDisplayUtilWin() {
  if (IsAPIAvailable()) {
    driver_controller_.Reset();
    if (virtual_displays_.size() > 0) {
      current_config_ = DriverProperties();
      StartWaiting();
    }
  }
  screen_->RemoveObserver(this);

  std::vector<display::Display> current_displays = screen_->GetAllDisplays();

  // Restore the display to the initial size and wait for displays to update.
  // This is necessary because Windows may change the resolution of the default
  // display (Microsoft Basic Display) while virtual displays exists and does
  // not automatically restore the original resolution.
  if (is_headless_) {
    CHECK_EQ(initial_displays_.size(), 1u);
    CHECK_EQ(current_displays.size(), 1u);
    const gfx::Size& initial_size = initial_displays_.front().size();
    bool set_resolution_result = SetDisplayResolution(initial_size);
    CHECK(set_resolution_result) << "SetDisplayResolution failed.";
    DisplayConfigWaiter waiter(screen_);
    waiter.WaitForDisplaySizes(std::vector<gfx::Size>{initial_size});

    // Basic check to ensure that the # of displays and resolutions match what
    // was in place when the utility was constructed. Helps prevent lingering
    // changes that may impact other tests running on the bot (i.e.
    // crbug.com/341931537).
    CHECK_EQ(current_displays.size(), initial_displays_.size())
        << "# of displays mismatch after driver was closed.";

    std::multiset<gfx::Size, SizeCompare> initial_sizes, current_sizes;
    for (const auto& display : screen_->GetAllDisplays()) {
      current_sizes.insert(display.size());
    }
    for (const auto& display : initial_displays_) {
      initial_sizes.insert(display.size());
    }
    CHECK(initial_sizes == current_sizes)
        << "Display resolution does not match initial config. Initial: "
        << JoinDisplayStrings(initial_displays_)
        << ", current: " << JoinDisplayStrings(current_displays);
  }
}

// static
bool VirtualDisplayUtilWin::IsAPIAvailable() {
  return DisplayDriverController::IsDriverInstalled();
}

int64_t VirtualDisplayUtilWin::AddDisplay(const DisplayParams& display_params) {
  uint8_t id = SynthesizeInternalDisplayId();
  if (virtual_displays_.find(id) != virtual_displays_.end()) {
    LOG(ERROR) << "Duplicate virtual display ID added: " << id;
    return kInvalidDisplayId;
  }
  std::vector<MonitorConfig> monitors = current_config_.requested_configs();
  auto it = kConfigMap.find(display_params);
  CHECK(it != kConfigMap.end()) << "DisplayParams not mapped to MonitorConfig.";
  MonitorConfig new_config = it->second;
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
    std::vector<MonitorConfig> configs{MonitorConfig::k1024x768};
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

void VirtualDisplayUtilWin::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  for (const auto& display : removed_displays) {
    base::EraseIf(virtual_displays_,
                  [&display](const std::pair<unsigned short, int64_t>& obj) {
                    return obj.second == display.id();
                  });
    OnDisplayAddedOrRemoved(display.id());
  }
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
    // While waiting for displays, also periodically ensure they are in extended
    // mode, so they they will become detected as new displays.
    ensure_extended_timer_.Start(FROM_HERE, base::Seconds(1), this,
                                 &VirtualDisplayUtilWin::EnsureExtendMode);
    run_loop_->Run();
  }
  run_loop_.reset();
}

void VirtualDisplayUtilWin::StopWaiting() {
  CHECK(run_loop_);
  ensure_extended_timer_.Stop();
  run_loop_->Quit();
}

void VirtualDisplayUtilWin::EnsureExtendMode() {
  if (current_config_.requested_configs().size() < 1) {
    // Don't set the topology if no displays were requested.
    return;
  }
  UINT32 path_array_size, mode_array_size = 0;
  GetDisplayConfigBufferSizes(QDC_ALL_PATHS, &path_array_size,
                              &mode_array_size);
  std::vector<DISPLAYCONFIG_PATH_INFO> paths(path_array_size);
  std::vector<DISPLAYCONFIG_MODE_INFO> modes(mode_array_size);
  DISPLAYCONFIG_TOPOLOGY_ID topology;
  LONG ret =
      QueryDisplayConfig(QDC_DATABASE_CURRENT, &path_array_size, paths.data(),
                         &mode_array_size, modes.data(), &topology);
  if (ret != ERROR_SUCCESS) {
    LOG(ERROR) << "QueryDisplayConfig failed: " << ret;
    return;
  }
  if (topology == DISPLAYCONFIG_TOPOLOGY_EXTEND) {
    ensure_extended_timer_.Stop();
    return;
  }
  ret =
      SetDisplayConfig(0, nullptr, 0, nullptr, SDC_TOPOLOGY_EXTEND | SDC_APPLY);
  if (ret == ERROR_SUCCESS) {
    ensure_extended_timer_.Stop();
    return;
  }
  LOG(ERROR) << "SetDisplayConfig failed: " << ret;
}

// static
uint8_t VirtualDisplayUtilWin::SynthesizeInternalDisplayId() {
  static uint8_t synthesized_display_id = 0;
  CHECK_LT(synthesized_display_id, std::numeric_limits<uint8_t>::max())
      << "All synthesized display IDs in use.";
  return synthesized_display_id++;
}

// static
std::unique_ptr<VirtualDisplayUtil> VirtualDisplayUtil::TryCreate(
    Screen* screen) {
  if (!VirtualDisplayUtilWin::IsAPIAvailable()) {
    return nullptr;
  }
  return std::make_unique<VirtualDisplayUtilWin>(screen);
}

DisplayConfigWaiter::DisplayConfigWaiter(Screen* screen) : screen_(screen) {
  screen_->AddObserver(this);
}

DisplayConfigWaiter::~DisplayConfigWaiter() {
  screen_->RemoveObserver(this);
}

void DisplayConfigWaiter::WaitForDisplaySizes(std::vector<gfx::Size> sizes) {
  wait_for_sizes_ = std::move(sizes);
  CHECK(!run_loop_);
  run_loop_ = std::make_unique<base::RunLoop>();
  if (!IsWaitConditionMet()) {
    run_loop_->Run();
  }
  run_loop_.reset();
}

void DisplayConfigWaiter::OnDisplayAdded(const display::Display& new_display) {
  if (IsWaitConditionMet()) {
    run_loop_->Quit();
  }
}

void DisplayConfigWaiter::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  if (IsWaitConditionMet()) {
    run_loop_->Quit();
  }
}

bool DisplayConfigWaiter::IsWaitConditionMet() {
  std::multiset<gfx::Size, SizeCompare> expected_sizes, current_sizes;
  for (display::Display display : screen_->GetAllDisplays()) {
    current_sizes.insert(display.size());
  }
  for (const auto& size : wait_for_sizes_) {
    expected_sizes.insert(size);
  }
  return expected_sizes == current_sizes;
}

}  // namespace display::test

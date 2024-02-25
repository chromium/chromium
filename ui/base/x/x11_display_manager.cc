// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_display_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/device_event_log/device_event_log.h"
#include "ui/base/x/x11_display_util.h"
#include "ui/display/types/display_config.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/randr.h"
#include "ui/gfx/x/xproto.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#endif

namespace ui {

namespace {

// Need at least xrandr version 1.3
constexpr std::pair<uint32_t, uint32_t> kMinXrandrVersion{1, 3};

}  // namespace

XDisplayManager::XDisplayManager(Delegate* delegate)
    : delegate_(delegate),
      displays_{display::Display()},
      connection_(x11::Connection::Get()),
      x_root_window_(connection_->default_screen().root),
      workspace_handler_(this) {}

XDisplayManager::~XDisplayManager() = default;

void XDisplayManager::Init() {
  if (IsXrandrAvailable()) {
    auto& randr = connection_->randr();
    randr.SelectInput(
        {x_root_window_, x11::RandR::NotifyMask::ScreenChange |
                             x11::RandR::NotifyMask::OutputChange |
                             x11::RandR::NotifyMask::CrtcChange});
  }
  FetchDisplayList();
}

// Need at least xrandr version 1.3
bool XDisplayManager::IsXrandrAvailable() const {
  return connection_->randr_version() >= kMinXrandrVersion;
}

const display::Display& XDisplayManager::GetPrimaryDisplay() const {
  DCHECK(!displays_.empty());
  return displays_[primary_display_index_];
}

void XDisplayManager::AddObserver(display::DisplayObserver* observer) {
  change_notifier_.AddObserver(observer);
}

void XDisplayManager::RemoveObserver(display::DisplayObserver* observer) {
  change_notifier_.RemoveObserver(observer);
}

void XDisplayManager::OnEvent(const x11::Event& xev) {
  auto* prop = xev.As<x11::PropertyNotifyEvent>();
  if (xev.As<x11::RandR::NotifyEvent>() ||
      (prop && prop->atom == x11::GetAtom("_NET_WORKAREA"))) {
    DispatchDelayedDisplayListUpdate();
  }
}

void XDisplayManager::SetDisplayList(std::vector<display::Display> displays,
                                     size_t primary_display_index) {
  displays_ = std::move(displays);
  primary_display_index_ = primary_display_index;
  delegate_->OnXDisplayListUpdated();
}

// Talks to xrandr to get the information of the outputs for a screen and
// updates display::Display list. The minimum required version of xrandr is
// 1.3.
void XDisplayManager::FetchDisplayList() {
  std::vector<display::Display> displays;
  display::DisplayConfig empty_display_config{
      display::Display::HasForceDeviceScaleFactor()
          ? display::Display::GetForcedDeviceScaleFactor()
          : 1.0f};
  const auto* display_config = &empty_display_config;
#if BUILDFLAG(IS_LINUX)
  if (const auto* linux_ui = ui::LinuxUi::instance()) {
    display_config = &linux_ui->display_config();
  }
#endif
  size_t primary_display_index = 0;
  if (IsXrandrAvailable()) {
    displays =
        BuildDisplaysFromXRandRInfo(*display_config, &primary_display_index);
  } else {
    displays = GetFallbackDisplayList(display_config->primary_scale,
                                      &primary_display_index);
  }

  if (displays != displays_) {
    DISPLAY_LOG(EVENT) << "Displays updated, count: " << displays.size();
    for (const auto& display : displays) {
      DISPLAY_LOG(EVENT) << display.ToString();
    }
  }

  SetDisplayList(std::move(displays), primary_display_index);
}

void XDisplayManager::OnCurrentWorkspaceChanged(
    const std::string& new_workspace) {
  change_notifier_.NotifyCurrentWorkspaceChanged(new_workspace);
}

void XDisplayManager::UpdateDisplayList() {
  std::vector<display::Display> old_displays = displays_;
  FetchDisplayList();
  change_notifier_.NotifyDisplaysChanged(old_displays, displays_);
}

void XDisplayManager::DispatchDelayedDisplayListUpdate() {
  update_task_.Reset(base::BindOnce(&XDisplayManager::UpdateDisplayList,
                                    base::Unretained(this)));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, update_task_.callback());
}

std::string XDisplayManager::GetCurrentWorkspace() {
  return workspace_handler_.GetCurrentWorkspace();
}

}  // namespace ui

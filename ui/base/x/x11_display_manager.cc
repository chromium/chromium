// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_display_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/device_event_log/device_event_log.h"
#include "ui/base/x/x11_display_util.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/randr.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/xproto.h"
#include "ui/linux/linux_ui.h"

namespace ui {

namespace {

constexpr int kMinXrandrVersion = 103;  // Need at least xrandr version 1.3

}  // namespace

XDisplayManager::XDisplayManager(Delegate* delegate)
    : delegate_(delegate),
      connection_(x11::Connection::Get()),
      x_root_window_(connection_->default_screen().root),
      xrandr_version_(GetXrandrVersion()),
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
  return xrandr_version_ >= kMinXrandrVersion;
}

display::Display XDisplayManager::GetPrimaryDisplay() const {
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

void XDisplayManager::SetDisplayList(std::vector<display::Display> displays) {
  displays_ = std::move(displays);
  delegate_->OnXDisplayListUpdated();
}

// Talks to xrandr to get the information of the outputs for a screen and
// updates display::Display list. The minimum required version of xrandr is
// 1.3.
void XDisplayManager::FetchDisplayList() {
  std::vector<display::Display> displays;
  auto& display_config = delegate_->GetDisplayConfig();
  if (IsXrandrAvailable()) {
    displays = BuildDisplaysFromXRandRInfo(xrandr_version_, display_config,
                                           &primary_display_index_);
  } else {
    displays = GetFallbackDisplayList(display_config.primary_scale);
  }

  if (displays != displays_) {
    DISPLAY_LOG(EVENT) << "Displays updated, count: " << displays.size();
    for (const auto& display : displays) {
      DISPLAY_LOG(EVENT) << display.ToString();
    }
  }

  SetDisplayList(std::move(displays));
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

gfx::Point XDisplayManager::GetCursorLocation() const {
  if (auto response = connection_->QueryPointer({x_root_window_}).Sync())
    return {response->root_x, response->root_y};
  return {};
}

std::string XDisplayManager::GetCurrentWorkspace() {
  return workspace_handler_.GetCurrentWorkspace();
}

}  // namespace ui

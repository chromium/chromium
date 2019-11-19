// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_display_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/base/x/x11_display_util.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"

namespace ui {

namespace {

constexpr int kMinXrandrVersion = 103;  // Need at least xrandr version 1.3
constexpr auto kDisplayListUpdateDelay = base::TimeDelta::FromMilliseconds(250);

}  // namespace

XDisplayManager::XDisplayManager(Delegate* delegate)
    : delegate_(delegate),
      xdisplay_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      xrandr_version_(GetXrandrVersion(xdisplay_)) {}

XDisplayManager::~XDisplayManager() = default;

void XDisplayManager::Init() {
  if (IsXrandrAvailable()) {
    int error_base_ignored = 0;
    XRRQueryExtension(xdisplay_, &xrandr_event_base_, &error_base_ignored);

    XRRSelectInput(xdisplay_, x_root_window_,
                   RRScreenChangeNotifyMask | RROutputChangeNotifyMask |
                       RRCrtcChangeNotifyMask);
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

bool XDisplayManager::CanProcessEvent(const XEvent& xev) {
  return xev.type - xrandr_event_base_ == RRScreenChangeNotify ||
         xev.type - xrandr_event_base_ == RRNotify ||
         (xev.type == PropertyNotify &&
          xev.xproperty.window == x_root_window_ &&
          xev.xproperty.atom == gfx::GetAtom("_NET_WORKAREA"));
}

bool XDisplayManager::ProcessEvent(XEvent* xev) {
  DCHECK(xev);
  int ev_type = xev->type - xrandr_event_base_;
  if (ev_type == RRScreenChangeNotify) {
    // Pass the event through to xlib.
    XRRUpdateConfiguration(xev);
    return true;
  }
  if (ev_type == RRNotify ||
      (xev->type == PropertyNotify &&
       xev->xproperty.atom == gfx::GetAtom("_NET_WORKAREA"))) {
    DispatchDelayedDisplayListUpdate();
    return true;
  }
  return false;
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
  float scale = delegate_->GetXDisplayScaleFactor();
  if (IsXrandrAvailable()) {
    displays = BuildDisplaysFromXRandRInfo(xrandr_version_, scale,
                                           &primary_display_index_);
  } else {
    displays = GetFallbackDisplayList(scale);
  }
  SetDisplayList(std::move(displays));
}

void XDisplayManager::UpdateDisplayList() {
  std::vector<display::Display> old_displays = displays_;
  FetchDisplayList();
  change_notifier_.NotifyDisplaysChanged(old_displays, displays_);
}

void XDisplayManager::DispatchDelayedDisplayListUpdate() {
  delayed_update_task_.Reset(base::BindOnce(&XDisplayManager::UpdateDisplayList,
                                            base::Unretained(this)));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, delayed_update_task_.callback(), kDisplayListUpdateDelay);
}

gfx::Point XDisplayManager::GetCursorLocation() const {
  XID root, child;
  int root_x, root_y, win_x, win_y;
  unsigned int mask;
  XQueryPointer(xdisplay_, x_root_window_, &root, &child, &root_x, &root_y,
                &win_x, &win_y, &mask);

  return gfx::Point(root_x, root_y);
}

}  // namespace ui

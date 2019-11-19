// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/x11_topmost_window_finder.h"

#include <stddef.h>

#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"

namespace views {

X11TopmostWindowFinder::X11TopmostWindowFinder() = default;

X11TopmostWindowFinder::~X11TopmostWindowFinder() = default;

aura::Window* X11TopmostWindowFinder::FindLocalProcessWindowAt(
    const gfx::Point& screen_loc_in_pixels,
    const std::set<aura::Window*>& ignore) {
  screen_loc_in_pixels_ = screen_loc_in_pixels;
  ignore_ = ignore;

  std::vector<aura::Window*> local_process_windows =
      DesktopWindowTreeHostLinux::GetAllOpenWindows();
  if (std::none_of(local_process_windows.cbegin(), local_process_windows.cend(),
                   [this](auto* window) {
                     return ShouldStopIteratingAtLocalProcessWindow(window);
                   }))
    return nullptr;

  ui::EnumerateTopLevelWindows(this);
  return DesktopWindowTreeHostLinux::GetContentWindowForWidget(
      static_cast<gfx::AcceleratedWidget>(toplevel_));
}

XID X11TopmostWindowFinder::FindWindowAt(
    const gfx::Point& screen_loc_in_pixels) {
  screen_loc_in_pixels_ = screen_loc_in_pixels;
  ui::EnumerateTopLevelWindows(this);
  return toplevel_;
}

bool X11TopmostWindowFinder::ShouldStopIterating(XID xid) {
  if (!ui::IsWindowVisible(xid))
    return false;

  auto* window = DesktopWindowTreeHostLinux::GetContentWindowForWidget(
      static_cast<gfx::AcceleratedWidget>(xid));
  if (window) {
    if (ShouldStopIteratingAtLocalProcessWindow(window)) {
      toplevel_ = xid;
      return true;
    }
    return false;
  }

  if (ui::WindowContainsPoint(xid, screen_loc_in_pixels_)) {
    toplevel_ = xid;
    return true;
  }
  return false;
}

bool X11TopmostWindowFinder::ShouldStopIteratingAtLocalProcessWindow(
    aura::Window* window) {
  if (ignore_.find(window) != ignore_.end())
    return false;

  // Currently |window|->IsVisible() always returns true.
  // TODO(pkotwicz): Fix this. crbug.com/353038
  if (!window->IsVisible())
    return false;

  auto* host = DesktopWindowTreeHostLinux::GetHostForWidget(
      window->GetHost()->GetAcceleratedWidget());
  if (!static_cast<DesktopWindowTreeHostX11*>(host)
           ->GetXRootWindowOuterBounds()
           .Contains(screen_loc_in_pixels_))
    return false;

  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(window->GetRootWindow());
  gfx::Point window_loc(screen_loc_in_pixels_);
  screen_position_client->ConvertPointFromScreen(window, &window_loc);
  return host->ContainsPointInXRegion(window_loc);
}

}  // namespace views

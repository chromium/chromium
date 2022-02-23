// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_topmost_window_finder.h"

#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "base/containers/adapters.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/xproto_util.h"
#include "ui/ozone/platform/x11/x11_window.h"
#include "ui/ozone/platform/x11/x11_window_manager.h"

namespace ui {

namespace {

using ShouldStopIteratingCallback = base::RepeatingCallback<bool(x11::Window)>;

// Returns true if |window| is a named window.
bool IsWindowNamed(x11::Window window) {
  return PropertyExists(window, x11::Atom::WM_NAME);
}

bool EnumerateChildren(ShouldStopIteratingCallback should_stop_iterating,
                       x11::Window window,
                       const int max_depth,
                       int depth) {
  if (depth > max_depth)
    return false;

  auto query_tree = x11::Connection::Get()->QueryTree({window}).Sync();
  if (!query_tree)
    return false;
  std::vector<x11::Window> windows = std::move(query_tree->children);

  // XQueryTree returns the children of |window| in bottom-to-top order, so
  // reverse-iterate the list to check the windows from top-to-bottom.
  for (const auto& window : base::Reversed(windows)) {
    if (depth < max_depth) {
      if (EnumerateChildren(should_stop_iterating, window, max_depth,
                            depth + 1))
        return true;
    }
    if (IsWindowNamed(window) && should_stop_iterating.Run(window))
      return true;
  }

  return false;
}

bool EnumerateAllWindows(ShouldStopIteratingCallback should_stop_iterating,
                         int max_depth) {
  x11::Window root = GetX11RootWindow();
  return EnumerateChildren(should_stop_iterating, root, max_depth, 0);
}

void EnumerateTopLevelWindows(
    ui::ShouldStopIteratingCallback should_stop_iterating) {
  // WMs may reparent toplevel windows inside their own containers, so extend
  // the search to all grandchildren of all toplevel windows.
  const int kMaxSearchDepth = 2;
  ui::EnumerateAllWindows(should_stop_iterating, kMaxSearchDepth);
}

}  // namespace

X11TopmostWindowFinder::X11TopmostWindowFinder(
    const std::set<gfx::AcceleratedWidget>& ignore)
    : ignore_(ignore) {}

X11TopmostWindowFinder::~X11TopmostWindowFinder() = default;

x11::Window X11TopmostWindowFinder::FindLocalProcessWindowAt(
    const gfx::Point& screen_loc_in_pixels) {
  screen_loc_in_pixels_ = screen_loc_in_pixels;

  std::vector<X11Window*> local_process_windows =
      X11WindowManager::GetInstance()->GetAllOpenWindows();
  if (std::none_of(local_process_windows.cbegin(), local_process_windows.cend(),
                   [this](auto* window) {
                     return ShouldStopIteratingAtLocalProcessWindow(window);
                   })) {
    return x11::Window::None;
  }

  EnumerateTopLevelWindows(base::BindRepeating(
      &X11TopmostWindowFinder::ShouldStopIterating, base::Unretained(this)));
  return toplevel_;
}

x11::Window X11TopmostWindowFinder::FindWindowAt(
    const gfx::Point& screen_loc_in_pixels) {
  screen_loc_in_pixels_ = screen_loc_in_pixels;
  EnumerateTopLevelWindows(base::BindRepeating(
      &X11TopmostWindowFinder::ShouldStopIterating, base::Unretained(this)));
  return toplevel_;
}

bool X11TopmostWindowFinder::ShouldStopIterating(x11::Window xwindow) {
  if (!IsWindowVisible(xwindow))
    return false;

  auto* window = X11WindowManager::GetInstance()->GetWindow(
      static_cast<gfx::AcceleratedWidget>(xwindow));
  if (window) {
    if (ShouldStopIteratingAtLocalProcessWindow(window)) {
      toplevel_ = xwindow;
      return true;
    }
    return false;
  }

  if (WindowContainsPoint(xwindow, screen_loc_in_pixels_)) {
    toplevel_ = xwindow;
    return true;
  }
  return false;
}

bool X11TopmostWindowFinder::ShouldStopIteratingAtLocalProcessWindow(
    X11Window* window) {
  if (ignore_.find(window->GetWidget()) != ignore_.end())
    return false;

  // Currently |window|->IsVisible() always returns true.
  // TODO(pkotwicz): Fix this. crbug.com/353038
  if (!window->IsVisible())
    return false;

  gfx::Rect window_bounds = window->GetOuterBounds();
  if (!window_bounds.Contains(screen_loc_in_pixels_))
    return false;

  gfx::Point window_point(screen_loc_in_pixels_);
  window_point.Offset(-window_bounds.origin().x(), -window_bounds.origin().y());
  return window->ContainsPointInXRegion(window_point);
}

}  // namespace ui

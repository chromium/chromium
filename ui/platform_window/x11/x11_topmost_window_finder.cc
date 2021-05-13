// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/x11_topmost_window_finder.h"

#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "ui/base/x/x11_menu_list.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/xproto_util.h"
#include "ui/platform_window/x11/x11_window.h"
#include "ui/platform_window/x11/x11_window_manager.h"

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

  std::vector<x11::Window> windows;
  if (depth == 0) {
    XMenuList::GetInstance()->InsertMenuWindows(&windows);
    // Enumerate the menus first.
    std::vector<x11::Window>::iterator iter;
    for (iter = windows.begin(); iter != windows.end(); iter++) {
      if (should_stop_iterating.Run(*iter))
        return true;
    }
    windows.clear();
  }

  auto query_tree = x11::Connection::Get()->QueryTree({window}).Sync();
  if (!query_tree)
    return false;
  windows = std::move(query_tree->children);

  // XQueryTree returns the children of |window| in bottom-to-top order, so
  // reverse-iterate the list to check the windows from top-to-bottom.
  std::vector<x11::Window>::reverse_iterator iter;
  for (iter = windows.rbegin(); iter != windows.rend(); iter++) {
    if (IsWindowNamed(*iter) && should_stop_iterating.Run(*iter))
      return true;
  }

  // If we're at this point, we didn't find the window we're looking for at the
  // current level, so we need to recurse to the next level.  We use a second
  // loop because the recursion and call to XQueryTree are expensive and is only
  // needed for a small number of cases.
  if (++depth <= max_depth) {
    for (iter = windows.rbegin(); iter != windows.rend(); iter++) {
      if (EnumerateChildren(should_stop_iterating, *iter, max_depth, depth))
        return true;
    }
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
  std::vector<x11::Window> stack;
  if (!ui::GetXWindowStack(ui::GetX11RootWindow(), &stack)) {
    // Window Manager doesn't support _NET_CLIENT_LIST_STACKING, so fall back
    // to old school enumeration of all X windows.  Some WMs parent 'top-level'
    // windows in unnamed actual top-level windows (ion WM), so extend the
    // search depth to all children of top-level windows.
    const int kMaxSearchDepth = 1;
    ui::EnumerateAllWindows(should_stop_iterating, kMaxSearchDepth);
    return;
  }
  XMenuList::GetInstance()->InsertMenuWindows(&stack);

  std::vector<x11::Window>::iterator iter;
  for (iter = stack.begin(); iter != stack.end(); iter++) {
    if (should_stop_iterating.Run(*iter))
      return;
  }
}

}  // namespace

X11TopmostWindowFinder::X11TopmostWindowFinder() = default;

X11TopmostWindowFinder::~X11TopmostWindowFinder() = default;

x11::Window X11TopmostWindowFinder::FindLocalProcessWindowAt(
    const gfx::Point& screen_loc_in_pixels,
    const std::set<gfx::AcceleratedWidget>& ignore) {
  screen_loc_in_pixels_ = screen_loc_in_pixels;
  ignore_ = ignore;

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

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

WaylandWindowManager::WaylandWindowManager() = default;

WaylandWindowManager::~WaylandWindowManager() = default;

void WaylandWindowManager::AddObserver(WaylandWindowObserver* observer) {
  observers_.AddObserver(observer);
}

void WaylandWindowManager::RemoveObserver(WaylandWindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WaylandWindowManager::NotifyWindowConfigured(WaylandWindow* window) {
  for (WaylandWindowObserver& observer : observers_)
    observer.OnWindowConfigured(window);
}

void WaylandWindowManager::GrabLocatedEvents(WaylandWindow* window) {
  DCHECK_NE(located_events_grabber_, window);

  // Wayland doesn't allow to grab the mouse. However, we start forwarding all
  // mouse events received by WaylandWindow to the aura::WindowEventDispatcher
  // which has capture.
  auto* old_grabber = located_events_grabber_;
  located_events_grabber_ = window;
  if (old_grabber)
    old_grabber->OnWindowLostCapture();
}

void WaylandWindowManager::UngrabLocatedEvents(WaylandWindow* window) {
  DCHECK_EQ(located_events_grabber_, window);
  auto* old_grabber = located_events_grabber_;
  located_events_grabber_ = nullptr;
  old_grabber->OnWindowLostCapture();
}

WaylandWindow* WaylandWindowManager::GetWindow(
    gfx::AcceleratedWidget widget) const {
  auto it = window_map_.find(widget);
  return it == window_map_.end() ? nullptr : it->second;
}

WaylandWindow* WaylandWindowManager::GetWindowWithLargestBounds() const {
  WaylandWindow* window_with_largest_bounds = nullptr;
  for (auto entry : window_map_) {
    if (!window_with_largest_bounds) {
      window_with_largest_bounds = entry.second;
      continue;
    }
    WaylandWindow* window = entry.second;
    if (window_with_largest_bounds->GetBounds() < window->GetBounds())
      window_with_largest_bounds = window;
  }
  return window_with_largest_bounds;
}

WaylandWindow* WaylandWindowManager::GetCurrentFocusedWindow() const {
  for (auto entry : window_map_) {
    WaylandWindow* window = entry.second;
    if (window->has_pointer_focus() || window->has_touch_focus())
      return window;
  }
  return nullptr;
}

WaylandWindow* WaylandWindowManager::GetCurrentKeyboardFocusedWindow() const {
  for (auto entry : window_map_) {
    WaylandWindow* window = entry.second;
    if (window->has_keyboard_focus())
      return window;
  }
  return nullptr;
}

WaylandWindow* WaylandWindowManager::FindParentForNewWindow(
    gfx::AcceleratedWidget parent_widget) const {
  auto* parent_window = GetWindow(parent_widget);

  // If propagated parent has already had a child, it means that |this| is a
  // submenu of a 3-dot menu. In aura, the parent of a 3-dot menu and its
  // submenu is the main native widget, which is the main window. In contrast,
  // Wayland requires a menu window to be a parent of a submenu window. Thus,
  // check if the suggested parent has a child. If yes, take its child as a
  // parent of |this|.
  // Another case is a notification window or a drop down window, which does not
  // have a parent in aura. In this case, take the current focused window as a
  // parent.
  if (!parent_window)
    parent_window = GetCurrentFocusedWindow();

  // If there is no current focused window, figure out the current active window
  // set by the Wayland server. Only one window at a time can be set as active.
  if (!parent_window) {
    auto windows = GetAllWindows();
    for (auto* window : windows) {
      if (window->IsActive()) {
        parent_window = window;
        break;
      }
    }
  }

  return parent_window ? parent_window->GetTopMostChildWindow() : nullptr;
}

std::vector<WaylandWindow*> WaylandWindowManager::GetWindowsOnOutput(
    uint32_t output_id) {
  std::vector<WaylandWindow*> result;
  for (auto entry : window_map_) {
    if (entry.second->entered_outputs_ids().count(output_id) > 0)
      result.push_back(entry.second);
  }
  return result;
}

void WaylandWindowManager::AddWindow(gfx::AcceleratedWidget widget,
                                     WaylandWindow* window) {
  window_map_[widget] = window;

  for (WaylandWindowObserver& observer : observers_)
    observer.OnWindowAdded(window);
}

void WaylandWindowManager::RemoveWindow(gfx::AcceleratedWidget widget) {
  auto* window = window_map_[widget];
  DCHECK(window);

  window_map_.erase(widget);

  for (WaylandWindowObserver& observer : observers_)
    observer.OnWindowRemoved(window);
}

void WaylandWindowManager::AddSubsurface(gfx::AcceleratedWidget widget,
                                         WaylandSubsurface* subsurface) {
  auto* window = window_map_[widget];
  DCHECK(window);

  for (WaylandWindowObserver& observer : observers_)
    observer.OnSubsurfaceAdded(window, subsurface);
}

void WaylandWindowManager::RemoveSubsurface(gfx::AcceleratedWidget widget,
                                            WaylandSubsurface* subsurface) {
  auto* window = window_map_[widget];
  DCHECK(window);

  for (WaylandWindowObserver& observer : observers_)
    observer.OnSubsurfaceRemoved(window, subsurface);
}

gfx::AcceleratedWidget WaylandWindowManager::AllocateAcceleratedWidget() {
  return ++last_accelerated_widget_;
}

std::vector<WaylandWindow*> WaylandWindowManager::GetAllWindows() const {
  std::vector<WaylandWindow*> result;
  for (auto entry : window_map_)
    result.push_back(entry.second);
  return result;
}

}  // namespace ui

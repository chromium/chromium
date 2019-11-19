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

std::vector<WaylandWindow*> WaylandWindowManager::GetAllWindows() const {
  std::vector<WaylandWindow*> result;
  for (auto entry : window_map_)
    result.push_back(entry.second);
  return result;
}

}  // namespace ui

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

#include "base/observer_list.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_drag_controller.h"

namespace ui {

WaylandWindowManager::WaylandWindowManager(WaylandConnection* connection)
    : connection_(connection) {}

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
    if (window_with_largest_bounds->GetBoundsInDIP() < window->GetBoundsInDIP())
      window_with_largest_bounds = window;
  }
  return window_with_largest_bounds;
}

WaylandWindow* WaylandWindowManager::GetCurrentActiveWindow() const {
  for (const auto& entry : window_map_) {
    WaylandWindow* window = entry.second;
    if (window->IsActive())
      return window;
  }
  return nullptr;
}

WaylandWindow* WaylandWindowManager::GetCurrentFocusedWindow() const {
  for (const auto& entry : window_map_) {
    WaylandWindow* window = entry.second;
    if (window->has_pointer_focus() || window->has_touch_focus() ||
        window->has_keyboard_focus())
      return window;
  }
  return nullptr;
}

WaylandWindow* WaylandWindowManager::GetCurrentPointerOrTouchFocusedWindow()
    const {
  // In case there is an ongoing window dragging session, favor the window
  // according to the active drag source.
  //
  // TODO(https://crbug.com/1317063): Apply the same logic to data drag sessions
  // too?
  if (auto drag_source = connection_->window_drag_controller()->drag_source()) {
    return *drag_source == WaylandWindowDragController::DragSource::kMouse
               ? GetCurrentPointerFocusedWindow()
               : GetCurrentTouchFocusedWindow();
  }

  for (const auto& entry : window_map_) {
    WaylandWindow* window = entry.second;
    if (window->has_pointer_focus() || window->has_touch_focus())
      return window;
  }
  return nullptr;
}

WaylandWindow* WaylandWindowManager::GetCurrentPointerFocusedWindow() const {
  for (const auto& entry : window_map_) {
    WaylandWindow* window = entry.second;
    if (window->has_pointer_focus())
      return window;
  }
  return nullptr;
}

WaylandWindow* WaylandWindowManager::GetCurrentTouchFocusedWindow() const {
  for (const auto& entry : window_map_) {
    WaylandWindow* window = entry.second;
    if (window->has_touch_focus())
      return window;
  }
  return nullptr;
}

WaylandWindow* WaylandWindowManager::GetCurrentKeyboardFocusedWindow() const {
  for (const auto& entry : window_map_) {
    WaylandWindow* window = entry.second;
    if (window->has_keyboard_focus())
      return window;
  }
  return nullptr;
}

void WaylandWindowManager::SetPointerFocusedWindow(WaylandWindow* window) {
  auto* old_focused_window = GetCurrentPointerFocusedWindow();
  if (window == old_focused_window)
    return;
  if (old_focused_window)
    old_focused_window->SetPointerFocus(false);
  if (window)
    window->SetPointerFocus(true);
}

void WaylandWindowManager::SetTouchFocusedWindow(WaylandWindow* window) {
  auto* old_focused_window = GetCurrentTouchFocusedWindow();
  if (window == old_focused_window)
    return;
  if (old_focused_window)
    old_focused_window->set_touch_focus(false);
  if (window)
    window->set_touch_focus(true);
}

void WaylandWindowManager::SetKeyboardFocusedWindow(WaylandWindow* window) {
  auto* old_focused_window = GetCurrentKeyboardFocusedWindow();
  if (window == old_focused_window)
    return;
  if (old_focused_window)
    old_focused_window->set_keyboard_focus(false);
  if (window)
    window->set_keyboard_focus(true);
  for (auto& observer : observers_)
    observer.OnKeyboardFocusedWindowChanged();
}

std::vector<WaylandWindow*> WaylandWindowManager::GetWindowsOnOutput(
    uint32_t output_id) {
  std::vector<WaylandWindow*> result;
  for (auto entry : window_map_) {
    if (entry.second->GetPreferredEnteredOutputId() == output_id)
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

  if (window->has_keyboard_focus()) {
    for (auto& observer : observers_)
      observer.OnKeyboardFocusedWindowChanged();
  }
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

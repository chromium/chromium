// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

#include <algorithm>

#include "base/check.h"
#include "base/observer_list.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/display/display.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_window_observer.h"

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
  observers_.Notify(&WaylandWindowObserver::OnWindowConfigured, window);
}

void WaylandWindowManager::NotifyWindowRoleAssigned(WaylandWindow* window) {
  observers_.Notify(&WaylandWindowObserver::OnWindowRoleAssigned, window);
}

void WaylandWindowManager::GrabLocatedEvents(WaylandWindow* window) {
  DCHECK_NE(located_events_grabber_, window);

  // Wayland doesn't allow to grab the mouse. However, we start forwarding all
  // mouse events received by WaylandWindow to the aura::WindowEventDispatcher
  // which has capture.
  auto* old_grabber = located_events_grabber_.get();
  located_events_grabber_ = window;
  if (old_grabber)
    old_grabber->OnWindowLostCapture();
}

void WaylandWindowManager::UngrabLocatedEvents(WaylandWindow* window) {
  DCHECK_EQ(located_events_grabber_, window);
  auto* old_grabber = located_events_grabber_.get();
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
    if (window == pointer_focused_window_ || window->has_touch_focus() ||
        window == keyboard_focused_window_) {
      return window;
    }
  }
  return nullptr;
}

WaylandWindow* WaylandWindowManager::GetCurrentPointerOrTouchFocusedWindow()
    const {
  // Might be nullptr if no input devices are available.
  if (connection_->window_drag_controller()) {
    // In case there is an ongoing window dragging session, favor the window
    // according to the active drag source.
    //
    // TODO(crbug.com/40222425): Apply the same logic to data drag
    // sessions too?
    if (auto drag_source =
            connection_->window_drag_controller()->drag_source()) {
      return *drag_source == mojom::DragEventSource::kMouse
                 ? GetCurrentPointerFocusedWindow()
                 : GetCurrentTouchFocusedWindow();
    }
  }

  for (const auto& entry : window_map_) {
    WaylandWindow* window = entry.second;
    if (window == pointer_focused_window_ || window->has_touch_focus())
      return window;
  }
  return nullptr;
}

WaylandWindow* WaylandWindowManager::GetCurrentPointerFocusedWindow() const {
#if DCHECK_IS_ON()
  bool found = !pointer_focused_window_;
  for (const auto& entry : window_map_) {
    WaylandWindow* window = entry.second;
    if (window == pointer_focused_window_) {
      found = true;
      break;
    }
  }
  DCHECK(found);
#endif
  return pointer_focused_window_;
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
#if DCHECK_IS_ON()
  bool found = !keyboard_focused_window_;
  for (const auto& entry : window_map_) {
    WaylandWindow* window = entry.second;
    if (window == keyboard_focused_window_) {
      found = true;
      break;
    }
  }
  DCHECK(found);
#endif
  return keyboard_focused_window_;
}

void WaylandWindowManager::SetPointerFocusedWindow(WaylandWindow* window) {
  auto* old_focused_window = GetCurrentPointerFocusedWindow();
  if (window == old_focused_window)
    return;
  if (old_focused_window)
    old_focused_window->OnPointerFocusChanged(false);
  pointer_focused_window_ = window;
  if (window)
    window->OnPointerFocusChanged(true);
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
  keyboard_focused_window_ = window;
  observers_.Notify(&WaylandWindowObserver::OnKeyboardFocusedWindowChanged);
}

void WaylandWindowManager::AddWindow(gfx::AcceleratedWidget widget,
                                     WaylandWindow* window) {
  window_map_[widget] = window;

  observers_.Notify(&WaylandWindowObserver::OnWindowAdded, window);
}

void WaylandWindowManager::RemoveWindow(gfx::AcceleratedWidget widget) {
  auto* window = window_map_[widget].get();
  DCHECK(window);

  window_map_.erase(widget);

  // Reset `pointer_focused_window_` and `keyboard_focused_window_` before
  // notifying any observers to make sure GetCurrentPointerFocusedWindow() and
  // GetCurrentKeyboardFocusedWindow() behave correctly. Especially the former
  // can be problematic if notifying WaylandWindowDragController that a window
  // has been removed before resetting `pointer_focused_window_`, because that
  // leads to WaylandEventSource::OnPointerButtonEvent() being called, which
  // then calls GetCurrentPointerFocusedWindow().
  if (window == pointer_focused_window_) {
    pointer_focused_window_ = nullptr;
  }
  if (window == keyboard_focused_window_) {
    keyboard_focused_window_ = nullptr;
    observers_.Notify(&WaylandWindowObserver::OnKeyboardFocusedWindowChanged);
  }

  observers_.Notify(&WaylandWindowObserver::OnWindowRemoved, window);
}

void WaylandWindowManager::AddSubsurface(gfx::AcceleratedWidget widget,
                                         WaylandSubsurface* subsurface) {
  auto* window = window_map_[widget].get();
  DCHECK(window);

  observers_.Notify(&WaylandWindowObserver::OnSubsurfaceAdded, window,
                    subsurface);
}

void WaylandWindowManager::RemoveSubsurface(gfx::AcceleratedWidget widget,
                                            WaylandSubsurface* subsurface) {
  auto* window = window_map_[widget].get();
  DCHECK(window);

  observers_.Notify(&WaylandWindowObserver::OnSubsurfaceRemoved, window,
                    subsurface);
}

void WaylandWindowManager::RecycleSubsurface(
    std::unique_ptr<WaylandSubsurface> subsurface) {
  // Reset the root window when the corresponding subsurface is invalid,
  // preventing it from receiving events.
  subsurface->wayland_surface()->UnsetRootWindow();
  subsurface_recycle_cache_ = std::move(subsurface);
}

gfx::AcceleratedWidget WaylandWindowManager::AllocateAcceleratedWidget() {
  return ++last_accelerated_widget_;
}

void WaylandWindowManager::DumpState(std::ostream& out) const {
  int i = 0;
  out << "WaylandWindowManager:" << std::endl;
  for (const auto& window : window_map_) {
    out << "  wayland_window[" << i++ << "]:";
    window.second->DumpState(out);
    out << std::endl;
  }
}

std::vector<WaylandWindow*> WaylandWindowManager::GetAllWindows() const {
  std::vector<WaylandWindow*> result;
  for (auto& entry : window_map_) {
    result.push_back(entry.second);
  }
  return result;
}

bool WaylandWindowManager::IsWindowValid(const WaylandWindow* window) const {
  for (auto& pair : window_map_) {
    if (pair.second == window)
      return true;
  }
  return false;
}

void WaylandWindowManager::SetFontScale(float new_font_scale) {
  if (new_font_scale == font_scale_) {
    return;
  }
  font_scale_ = new_font_scale;
  for (WaylandWindow* window : GetAllWindows()) {
    window->OnFontScaleFactorChanged();
  }
}

float WaylandWindowManager::DetermineUiScale() const {
  using display::Display;
  constexpr float kMinUiScale = 0.5f;
  constexpr float kMaxUiScale = 3.0f;
  CHECK(connection_->IsUiScaleEnabled() || font_scale_ == 1.0f) << font_scale_;

  const float ui_scale =
      Display::HasForceDeviceScaleFactor() && connection_->IsUiScaleEnabled()
          ? Display::GetForcedDeviceScaleFactor()
          : font_scale_;
  return std::clamp(ui_scale, kMinUiScale, kMaxUiScale);
}

}  // namespace ui

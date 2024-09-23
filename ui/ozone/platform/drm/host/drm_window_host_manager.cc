// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_window_host_manager.h"

#include "base/check.h"
#include "base/notreached.h"
#include "ui/ozone/platform/drm/host/drm_window_host.h"

namespace ui {

DrmWindowHostManager::DrmWindowHostManager() {
}

DrmWindowHostManager::~DrmWindowHostManager() {
}

gfx::AcceleratedWidget DrmWindowHostManager::NextAcceleratedWidget() {
  // We're not using 0 since other code assumes that a 0 AcceleratedWidget is an
  // invalid widget.
  return ++last_allocated_widget_;
}

void DrmWindowHostManager::AddWindow(gfx::AcceleratedWidget widget,
                                     DrmWindowHost* window) {
  std::pair<WidgetToWindowMap::iterator, bool> result = window_map_.insert(
      std::pair<gfx::AcceleratedWidget, DrmWindowHost*>(widget, window));
  DCHECK(result.second) << "Window for " << widget << " already added.";
}

void DrmWindowHostManager::RemoveWindow(gfx::AcceleratedWidget widget) {
  WidgetToWindowMap::iterator it = window_map_.find(widget);
  if (it != window_map_.end()) {
    if (window_mouse_currently_on_ == it->second)
      window_mouse_currently_on_ = nullptr;
    window_map_.erase(it);
  } else {
    NOTREACHED_IN_MIGRATION()
        << "Attempting to remove non-existing window " << widget;
  }

  if (event_grabber_ == widget)
    event_grabber_ = gfx::kNullAcceleratedWidget;
}

DrmWindowHost* DrmWindowHostManager::GetWindow(gfx::AcceleratedWidget widget) {
  WidgetToWindowMap::iterator it = window_map_.find(widget);
  if (it != window_map_.end())
    return it->second;

  NOTREACHED_IN_MIGRATION()
      << "Attempting to get non-existing window " << widget;
  return NULL;
}

DrmWindowHost* DrmWindowHostManager::GetWindowAt(const gfx::Point& location) {
  for (auto it = window_map_.begin(); it != window_map_.end(); ++it)
    if (it->second->GetBoundsInPixels().Contains(location))
      return it->second;

  return NULL;
}

DrmWindowHost* DrmWindowHostManager::GetPrimaryWindow() {
  auto it = window_map_.begin();
  return it != window_map_.end() ? it->second : nullptr;
}

void DrmWindowHostManager::GrabEvents(gfx::AcceleratedWidget widget) {
  if (event_grabber_ != gfx::kNullAcceleratedWidget)
    return;
  event_grabber_ = widget;
}

void DrmWindowHostManager::UngrabEvents(gfx::AcceleratedWidget widget) {
  if (event_grabber_ != widget)
    return;
  event_grabber_ = gfx::kNullAcceleratedWidget;
}

void DrmWindowHostManager::MouseOnWindow(DrmWindowHost* window) {
  if (window_mouse_currently_on_ == window)
    return;
  window_mouse_currently_on_ = window;
  window->OnMouseEnter();
}

}  // namespace ui

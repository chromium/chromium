// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window_observer.h"

namespace ui {

WaylandWindowObserver::~WaylandWindowObserver() = default;

void WaylandWindowObserver::OnWindowAdded(WaylandWindow* window) {}

void WaylandWindowObserver::OnWindowRemoved(WaylandWindow* window) {}

void WaylandWindowObserver::OnWindowConfigured(WaylandWindow* window) {}

void WaylandWindowObserver::OnWindowRoleAssigned(WaylandWindow* window) {}

void WaylandWindowObserver::OnSubsurfaceAdded(WaylandWindow* window,
                                              WaylandSubsurface* subsurface) {}

void WaylandWindowObserver::OnSubsurfaceRemoved(WaylandWindow* window,
                                                WaylandSubsurface* subsurface) {
}

void WaylandWindowObserver::OnKeyboardFocusedWindowChanged() {}

}  // namespace ui

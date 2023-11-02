// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_OBSERVER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_OBSERVER_H_

#include "base/observer_list_types.h"

namespace ui {

class WaylandWindow;
class WaylandSubsurface;

// Observers for window management notifications.
class WaylandWindowObserver : public base::CheckedObserver {
 public:
  // Called when |window| has been added.
  virtual void OnWindowAdded(WaylandWindow* window);

  // Called when |window| has been removed.
  virtual void OnWindowRemoved(WaylandWindow* window);

  // Called when |window| has been ack configured.
  virtual void OnWindowConfigured(WaylandWindow* window);

  // Called when |window| has been assigned a role.
  virtual void OnWindowRoleAssigned(WaylandWindow* window);

  // Called when |window| adds |subsurface|.
  virtual void OnSubsurfaceAdded(WaylandWindow* window,
                                 WaylandSubsurface* subsurface);

  // Called when |window| removes |subsurface|.
  virtual void OnSubsurfaceRemoved(WaylandWindow* window,
                                   WaylandSubsurface* subsurface);

  // Called when the keyboard focused window is changed.
  // The latest keyboard focused window can be obtain via
  // WaylandWindowManager::GetCurrentKeyboardFocusedWindow().
  virtual void OnKeyboardFocusedWindowChanged();

 protected:
  ~WaylandWindowObserver() override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_OBSERVER_H_

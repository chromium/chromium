// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_OBSERVER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_OBSERVER_H_

#include "base/observer_list_types.h"

namespace ui {

class WaylandWindow;

// Observers for window management notifications.
class WaylandWindowObserver : public base::CheckedObserver {
 public:
  // Called when |window| has been added.
  virtual void OnWindowAdded(WaylandWindow* window);

  // Called when |window| has been removed.
  virtual void OnWindowRemoved(WaylandWindow* window);

 protected:
  ~WaylandWindowObserver() override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_OBSERVER_H_

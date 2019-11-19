// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_WINDOW_HOST_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_WINDOW_HOST_MANAGER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
}

namespace ui {

class DrmWindowHost;

// Responsible for keeping the mapping between the allocated widgets and
// windows.
class DrmWindowHostManager {
 public:
  DrmWindowHostManager();
  ~DrmWindowHostManager();

  gfx::AcceleratedWidget NextAcceleratedWidget();

  // Adds a window for |widget|. Note: |widget| should not be associated when
  // calling this function.
  void AddWindow(gfx::AcceleratedWidget widget, DrmWindowHost* window);

  // Removes the window association for |widget|. Note: |widget| must be
  // associated with a window when calling this function.
  void RemoveWindow(gfx::AcceleratedWidget widget);

  // Returns the window associated with |widget|. Note: This function should
  // only be called if a valid window has been associated.
  DrmWindowHost* GetWindow(gfx::AcceleratedWidget widget);

  // Returns the window containing the specified screen location, or NULL.
  DrmWindowHost* GetWindowAt(const gfx::Point& location);

  // Returns a window. Probably the first one created.
  DrmWindowHost* GetPrimaryWindow();

  // Tries to set a given widget as the recipient for events. It will
  // fail if there is already another widget as recipient.
  void GrabEvents(gfx::AcceleratedWidget widget);

  // Unsets a given widget as the recipient for events.
  void UngrabEvents(gfx::AcceleratedWidget widget);

  // Called when a mouse physicall moved into the |window|.
  void MouseOnWindow(DrmWindowHost* window);

  // Gets the current widget recipient of mouse events.
  gfx::AcceleratedWidget event_grabber() const { return event_grabber_; }

 private:
  typedef std::map<gfx::AcceleratedWidget, DrmWindowHost*> WidgetToWindowMap;

  gfx::AcceleratedWidget last_allocated_widget_ = 0;
  WidgetToWindowMap window_map_;
  DrmWindowHost* window_mouse_currently_on_ = nullptr;

  gfx::AcceleratedWidget event_grabber_ = gfx::kNullAcceleratedWidget;

  DISALLOW_COPY_AND_ASSIGN(DrmWindowHostManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_WINDOW_HOST_MANAGER_H_

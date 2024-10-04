// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_MANAGER_H_

#include <memory>
#include <ostream>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"
#include "ui/ozone/platform/wayland/host/wayland_window_observer.h"

namespace ui {

class WaylandWindow;
class WaylandSubsurface;
class WaylandConnection;

// Stores and returns WaylandWindows. Clients that are interested in knowing
// when a new window is added or removed, but set self as an observer.
class WaylandWindowManager {
 public:
  explicit WaylandWindowManager(WaylandConnection* connection);
  WaylandWindowManager(const WaylandWindowManager&) = delete;
  WaylandWindowManager& operator=(const WaylandWindowManager&) = delete;
  ~WaylandWindowManager();

  void AddObserver(WaylandWindowObserver* observer);
  void RemoveObserver(WaylandWindowObserver* observer);

  // Notifies observers that the Window has been ack configured and
  // WaylandBufferManagerHost can start attaching buffers to the |surface_|.
  void NotifyWindowConfigured(WaylandWindow* window);

  // Notifies observers that the window's wayland role has been assigned.
  void NotifyWindowRoleAssigned(WaylandWindow* window);

  // Stores the window that should grab the located events.
  void GrabLocatedEvents(WaylandWindow* event_grabber);

  // Removes the window that should grab the located events.
  void UngrabLocatedEvents(WaylandWindow* event_grabber);

  // Returns current event grabber.
  WaylandWindow* located_events_grabber() const {
    return located_events_grabber_;
  }

  // Returns a window found by |widget|.
  WaylandWindow* GetWindow(gfx::AcceleratedWidget widget) const;

  // Returns a window with largests bounds.
  WaylandWindow* GetWindowWithLargestBounds() const;

  // Returns a current active window.
  WaylandWindow* GetCurrentActiveWindow() const;

  // Returns a current focused window by pointer, touch, or keyboard.
  WaylandWindow* GetCurrentFocusedWindow() const;

  // Returns a current focused window by pointer or touch.
  WaylandWindow* GetCurrentPointerOrTouchFocusedWindow() const;

  // Returns a current focused window by pointer.
  WaylandWindow* GetCurrentPointerFocusedWindow() const;

  // Returns a current focused window by touch.
  WaylandWindow* GetCurrentTouchFocusedWindow() const;

  // Returns a current focused window by keyboard.
  WaylandWindow* GetCurrentKeyboardFocusedWindow() const;

  // Sets the given window as the pointer focused window.
  // If there already is another, the old one will be unset.
  // If nullptr is passed to |window|, it means pointer focus is unset from
  // any window.
  // The given |window| must be managed by this manager.
  void SetPointerFocusedWindow(WaylandWindow* window);

  // Sets the given window as the touch focused window.
  // If there already is another, the old one will be unset.
  // If nullptr is passed to |window|, it means touch focus is unset from
  // any window.
  // The given |window| must be managed by this manager.
  void SetTouchFocusedWindow(WaylandWindow* window);

  // Sets the given window as the keyboard focused window.
  // If there already is another, the old one will be unset.
  // If nullptr is passed to |window|, it means keyboard focus is unset from
  // any window.
  // The given |window| must be managed by this manager.
  void SetKeyboardFocusedWindow(WaylandWindow* window);

  // Returns all stored windows.
  std::vector<WaylandWindow*> GetAllWindows() const;

  // Returns true if the |window| still exists.
  bool IsWindowValid(const WaylandWindow* window) const;

  void AddWindow(gfx::AcceleratedWidget widget, WaylandWindow* window);
  void RemoveWindow(gfx::AcceleratedWidget widget);
  void AddSubsurface(gfx::AcceleratedWidget widget,
                     WaylandSubsurface* subsurface);
  void RemoveSubsurface(gfx::AcceleratedWidget widget,
                        WaylandSubsurface* subsurface);

  void RecycleSubsurface(std::unique_ptr<WaylandSubsurface> subsurface);

  // Creates a new unique gfx::AcceleratedWidget.
  gfx::AcceleratedWidget AllocateAcceleratedWidget();

  // Returns the current value that to be used as windows' UI scale. If UI
  // scaling feature is disabled or unavailable (eg: per-surface scaling
  // unsupported), 1 is returned. If present, the value passed in through the
  // force-device-scale-factor switch is used, otherwise the current font scale
  // is returned, which presumably comes from system's "text scaling factor"
  // setting, provided by LinuxUi, and set via SetFontScale function below.
  float DetermineUiScale() const;
  void SetFontScale(float new_font_scale);

  void DumpState(std::ostream& out) const;

 private:
  raw_ptr<WaylandWindow> pointer_focused_window_ = nullptr;
  raw_ptr<WaylandWindow> keyboard_focused_window_ = nullptr;

  const raw_ptr<WaylandConnection> connection_;

  base::ObserverList<WaylandWindowObserver> observers_;

  base::flat_map<gfx::AcceleratedWidget,
                 raw_ptr<WaylandWindow, CtnExperimental>>
      window_map_;

  // The cache of |primary_subsurface_| of the last closed WaylandWindow. This
  // will be destroyed lazily to make sure the window closing animation works
  // well. See crbug.com/1324548.
  std::unique_ptr<WaylandSubsurface> subsurface_recycle_cache_;

  raw_ptr<WaylandWindow> located_events_grabber_ = nullptr;

  // Stores strictly monotonically increasing counter for allocating unique
  // AccelerateWidgets.
  gfx::AcceleratedWidget last_accelerated_widget_ = gfx::kNullAcceleratedWidget;

  // Current system's text font scaling factor provided by WaylandScreen,
  // through LinuxUi, when enabled.
  float font_scale_ = 1.0f;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_MANAGER_H_

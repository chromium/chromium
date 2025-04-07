// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_EXTENSIONS_WAYLAND_EXTENSION_H_
#define UI_PLATFORM_WINDOW_EXTENSIONS_WAYLAND_EXTENSION_H_

#include "base/component_export.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

class PlatformWindow;

class COMPONENT_EXPORT(PLATFORM_WINDOW) WaylandExtension {
 public:
  // Waits for a Wayland roundtrip to ensure all side effects have been
  // processed.
  virtual void RoundTripQueue() = 0;

  // Returns true if there are any in flight requests for state updates.
  virtual bool HasInFlightRequestsForState() const = 0;

  // Returns the latest viz sequence ID for the currently applied state.
  virtual int64_t GetVizSequenceIdForAppliedState() const = 0;

  // Returns the latest viz sequence ID for the currently latched state.
  virtual int64_t GetVizSequenceIdForLatchedState() const = 0;

  // Sets whether we should latch state requests immediately, or wait for the
  // server to respond. See the comments on `latch_immediately_for_testing_` in
  // `WaylandWindow` for more details.
  virtual void SetLatchImmediately(bool latch_immediately) = 0;

 protected:
  virtual ~WaylandExtension();

  // Sets the pointer to the extension as a property of the PlatformWindow.
  void SetWaylandExtension(PlatformWindow* window, WaylandExtension* extension);
};

COMPONENT_EXPORT(PLATFORM_WINDOW)
WaylandExtension* GetWaylandExtension(const PlatformWindow& window);

class COMPONENT_EXPORT(PLATFORM_WINDOW) WaylandToplevelExtension {
 public:
  // Starts a window dragging session from the owning platform window triggered
  // by `event_source` (kMouse or kTouch) if it is not running yet. Under
  // Wayland, window dragging is backed by a platform drag-and-drop session.
  // |allow_system_drag| indicates whether it is allowed to use a regular
  // drag-and-drop session if the compositor does not support the extended-drag
  // protocol needed to implement all window dragging features.
  virtual void StartWindowDraggingSessionIfNeeded(
      ui::mojom::DragEventSource event_source,
      bool allow_system_drag) = 0;

  // Whether or not the underlying platform supports native pointer locking.
  virtual bool SupportsPointerLock() = 0;
  virtual void LockPointer(bool enabled) = 0;

  // Associates a dbus appmenu that has the specified service name and the
  // object path with this toplevel. The dbus appmenu implements the
  // com.canonical.dbusmenu interface.
  virtual void SetAppmenu(const std::string& service_name,
                          const std::string& object_path) = 0;

  // Unsets the appmenu associated with this toplevel.
  virtual void UnsetAppmenu() = 0;

 protected:
  virtual ~WaylandToplevelExtension();

  // Sets the pointer to the extension as a property of the PlatformWindow.
  void SetWaylandToplevelExtension(PlatformWindow* window,
                                   WaylandToplevelExtension* extension);
};

COMPONENT_EXPORT(PLATFORM_WINDOW)
WaylandToplevelExtension* GetWaylandToplevelExtension(
    const PlatformWindow& window);

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_EXTENSIONS_WAYLAND_EXTENSION_H_

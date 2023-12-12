// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_WINDOW_EVENT_MANAGER_H_
#define UI_GFX_X_WINDOW_EVENT_MANAGER_H_

#include <map>

#include "base/component_export.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

class WindowEventManager;

// Ensures events in |event_mask| are selected on |window| for the duration of
// this object's lifetime.
class COMPONENT_EXPORT(X11) ScopedEventSelector {
 public:
  ScopedEventSelector();

  ScopedEventSelector(ScopedEventSelector&& other);

  ScopedEventSelector& operator=(ScopedEventSelector&& other);

  ~ScopedEventSelector();

  void Reset();

 private:
  // Allow Connection to call the private constructor.
  friend class Connection;

  ScopedEventSelector(Connection* connection,
                      Window window,
                      EventMask event_mask);

  void Swap(ScopedEventSelector& other);

  Window window_ = Window::None;
  EventMask event_mask_ = EventMask::NoEvent;
  raw_ptr<WindowEventManager> event_manager_ = nullptr;
};

// Allows multiple clients within Chrome to select events on the same X window.
class WindowEventManager {
 public:
  explicit WindowEventManager(Connection* connection);

  WindowEventManager(const WindowEventManager&) = delete;
  WindowEventManager& operator=(const WindowEventManager&) = delete;

  ~WindowEventManager();

  void Reset();

 private:
  friend class ScopedEventSelector;

  class MultiMask;

  // Guarantees that events in |event_mask| will be reported to Chrome.
  void SelectEvents(Window window, EventMask event_mask);

  // Deselects events on |event_mask|.  Chrome will stop receiving events for
  // any set bit in |event_mask| only if no other client has selected that bit.
  void DeselectEvents(Window window, EventMask event_mask);

  // Helper method called by SelectEvents and DeselectEvents whenever the mask
  // corresponding to |window| might have changed.  Calls SetEventMask if
  // necessary.
  void AfterMaskChanged(Window window, EventMask old_mask);

  raw_ptr<Connection> connection_;

  std::map<Window, std::unique_ptr<MultiMask>> mask_map_;
};

}  // namespace x11

#endif  // UI_GFX_X_WINDOW_EVENT_MANAGER_H_

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_PLATFORM_X11_X11_HOTPLUG_EVENT_HANDLER_H_
#define UI_EVENTS_PLATFORM_X11_X11_HOTPLUG_EVENT_HANDLER_H_

#include "ui/events/devices/x11/device_list_cache_x11.h"

namespace ui {

// Parses X11 native devices and propagates the list of active devices to an
// observer.
class X11HotplugEventHandler {
 public:
  X11HotplugEventHandler();

  X11HotplugEventHandler(const X11HotplugEventHandler&) = delete;
  X11HotplugEventHandler& operator=(const X11HotplugEventHandler&) = delete;

  ~X11HotplugEventHandler();

  // Called on an X11 hotplug event.
  void OnHotplugEvent();
};

}  // namespace ui

#endif  // UI_EVENTS_PLATFORM_X11_X11_HOTPLUG_EVENT_HANDLER_H_

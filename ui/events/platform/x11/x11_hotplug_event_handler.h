// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_PLATFORM_X11_X11_HOTPLUG_EVENT_HANDLER_H_
#define UI_EVENTS_PLATFORM_X11_X11_HOTPLUG_EVENT_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/memory/weak_ptr.h"
#include "ui/events/devices/x11/device_list_cache_x11.h"
#include "ui/gfx/x/xinput.h"

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

 private:
  struct PendingHotplug;

  void OnDeviceNodeResponse(uint64_t serial,
                            size_t index,
                            x11::Input::XIGetPropertyResponse response);
  void FinishHotplugEvent();

  uint64_t next_serial_ = 0;
  std::unique_ptr<PendingHotplug> pending_hotplug_;
  base::WeakPtrFactory<X11HotplugEventHandler> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_EVENTS_PLATFORM_X11_X11_HOTPLUG_EVENT_HANDLER_H_

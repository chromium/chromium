// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_THREAD_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_THREAD_EVDEV_H_

#include <memory>

#include "base/functional/callback.h"
#include "ui/events/ozone/evdev/input_controller_evdev.h"

namespace base {
class Thread;
}

namespace ui {

class CursorDelegateEvdev;
class DeviceEventDispatcherEvdev;
class InputDeviceFactoryEvdevProxy;

typedef base::OnceCallback<void(std::unique_ptr<InputDeviceFactoryEvdevProxy>)>
    EventThreadStartCallback;

// Wrapper for events thread.
class EventThreadEvdev {
 public:
  EventThreadEvdev();

  EventThreadEvdev(const EventThreadEvdev&) = delete;
  EventThreadEvdev& operator=(const EventThreadEvdev&) = delete;

  ~EventThreadEvdev();

  // Start a new events thread. All device events will get sent to the
  // passed dispatcher. The thread will call directly into cursor, so it
  // must be synchronized accordingly.
  void Start(std::unique_ptr<DeviceEventDispatcherEvdev> dispatcher,
             CursorDelegateEvdev* cursor,
             EventThreadStartCallback callback,
             InputControllerEvdev* input_controller);

 private:
  std::unique_ptr<base::Thread> thread_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_THREAD_EVDEV_H_

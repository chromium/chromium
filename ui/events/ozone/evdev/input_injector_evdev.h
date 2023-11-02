// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_INPUT_INJECTOR_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_INPUT_INJECTOR_EVDEV_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/event_constants.h"
#include "ui/events/ozone/evdev/event_dispatch_callback.h"
#include "ui/ozone/public/system_input_injector.h"

namespace ui {

class CursorDelegateEvdev;
class DeviceEventDispatcherEvdev;

class COMPONENT_EXPORT(EVDEV) InputInjectorEvdev : public SystemInputInjector {
 public:
  InputInjectorEvdev(std::unique_ptr<DeviceEventDispatcherEvdev> dispatcher,
                     CursorDelegateEvdev* cursor);

  InputInjectorEvdev(const InputInjectorEvdev&) = delete;
  InputInjectorEvdev& operator=(const InputInjectorEvdev&) = delete;

  ~InputInjectorEvdev() override;

  // SystemInputInjector implementation.
  void SetDeviceId(int device_id) override;
  void InjectMouseButton(EventFlags button, bool down) override;
  void InjectMouseWheel(int delta_x, int delta_y) override;
  void MoveCursorTo(const gfx::PointF& location) override;
  void InjectKeyEvent(DomCode physical_key,
                      bool down,
                      bool suppress_auto_repeat) override;

 private:
  // Shared cursor state.
  const raw_ptr<CursorDelegateEvdev> cursor_;

  int device_id_ = ED_UNKNOWN_DEVICE;

  // Interface for dispatching events.
  const std::unique_ptr<DeviceEventDispatcherEvdev> dispatcher_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_INPUT_INJECTOR_EVDEV_H_

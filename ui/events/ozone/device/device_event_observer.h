// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_DEVICE_DEVICE_EVENT_OBSERVER_H_
#define UI_EVENTS_OZONE_DEVICE_DEVICE_EVENT_OBSERVER_H_

#include "base/component_export.h"

namespace ui {

class DeviceEvent;

class COMPONENT_EXPORT(EVENTS_OZONE) DeviceEventObserver {
 public:
  virtual ~DeviceEventObserver() {}

  virtual void OnDeviceEvent(const DeviceEvent& event) = 0;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_DEVICE_DEVICE_EVENT_OBSERVER_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_DEVICE_DEVICE_EVENT_H_
#define UI_EVENTS_OZONE_DEVICE_DEVICE_EVENT_H_

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace ui {

class COMPONENT_EXPORT(EVENTS_OZONE) DeviceEvent {
 public:
  enum DeviceType {
    INPUT,
    DISPLAY,
  };

  enum ActionType {
    ADD,
    REMOVE,
    CHANGE,
  };

  DeviceEvent(DeviceType type, ActionType action, const base::FilePath& path);

  DeviceEvent(const DeviceEvent&) = delete;
  DeviceEvent& operator=(const DeviceEvent&) = delete;

  DeviceType device_type() const { return device_type_; }
  ActionType action_type() const { return action_type_; }
  base::FilePath path() const { return path_; }

 private:
  DeviceType device_type_;
  ActionType action_type_;
  base::FilePath path_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_DEVICE_DEVICE_EVENT_H_

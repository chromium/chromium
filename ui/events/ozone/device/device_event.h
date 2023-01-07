// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_DEVICE_DEVICE_EVENT_H_
#define UI_EVENTS_OZONE_DEVICE_DEVICE_EVENT_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"

namespace ui {
namespace {

using PropertyMap = base::flat_map<std::string, std::string>;

}  // namespace

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

  DeviceEvent(DeviceType type,
              ActionType action,
              const base::FilePath& path,
              const PropertyMap& property_map = PropertyMap());

  DeviceEvent(const DeviceEvent&) = delete;
  DeviceEvent& operator=(const DeviceEvent&) = delete;
  ~DeviceEvent();

  DeviceType device_type() const { return device_type_; }
  ActionType action_type() const { return action_type_; }
  base::FilePath path() const { return path_; }
  const PropertyMap properties() const { return properties_; }

 private:
  DeviceType device_type_;
  ActionType action_type_;
  base::FilePath path_;
  const PropertyMap properties_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_DEVICE_DEVICE_EVENT_H_

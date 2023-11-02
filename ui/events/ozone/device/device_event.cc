// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/device/device_event.h"

namespace ui {

DeviceEvent::DeviceEvent(DeviceType type,
                         ActionType action,
                         const base::FilePath& path,
                         const PropertyMap& property_map)
    : device_type_(type),
      action_type_(action),
      path_(path),
      properties_(property_map) {}

DeviceEvent::~DeviceEvent() = default;

}  // namespace ui
